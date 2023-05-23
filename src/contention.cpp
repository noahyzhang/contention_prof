#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <pthread.h>
#include <atomic>
#include <mutex>
#include <memory>
#include "common/common.h"
#include "sample.h"
#include "collector.h"
#include "common/object_pool.h"
#include "common/log.h"
#include "profiler.h"
#include "contention.h"

namespace contention_prof {

// 锁操作的函数类型
typedef int (*pthread_mutex_lock_func_type)(pthread_mutex_t *mutex);
typedef int (*pthread_mutex_unlock_func_type)(pthread_mutex_t *mutex);

// 定义锁操作的系统接口
static pthread_mutex_lock_func_type real_pthread_mutex_lock_func = nullptr;
static pthread_mutex_unlock_func_type real_pthread_mutex_unlock_func = nullptr;

// 初始化函数
void mutex_hook_init() {
    real_pthread_mutex_lock_func = (pthread_mutex_lock_func_type)dlsym(RTLD_NEXT, "pthread_mutex_lock");
    real_pthread_mutex_unlock_func = (pthread_mutex_unlock_func_type)dlsym(RTLD_NEXT, "pthread_mutex_unlock");
}

const int TLS_MAX_COUNT = 3;

struct MutexAndContentionSite {
    pthread_mutex_t* mutex;
    pthread_contention_site_t csite;
};

struct TLSPthreadContentionSites {
    int count;
    uint64_t cp_version;
    MutexAndContentionSite list[TLS_MAX_COUNT];
};

static __thread TLSPthreadContentionSites tls_csites = {0, 0, {}};
static __thread bool tls_inside_lock = false;

const size_t MUTEX_MAP_SIZE = 1024;
struct MutexMapEntry {
    std::atomic<uint64_t> versioned_mutex;
    pthread_contention_site_t csite;
};
static MutexMapEntry g_mutex_map[MUTEX_MAP_SIZE] = {};

bool is_contention_site_valid(const pthread_contention_site_t& cs) {
    return cs.sampling_range;
}

void make_contention_site_invalid(pthread_contention_site_t* cs) {
    cs->sampling_range = 0;
}

inline uint64_t hash_mutex_ptr(const pthread_mutex_t* m) {
    return Util::fmix64((uint64_t)m);
}

const int PTR_BITS = 48;

pthread_contention_site_t* add_pthread_contention_site(pthread_mutex_t* mutex) {
    MutexMapEntry& entry = g_mutex_map[hash_mutex_ptr(mutex) & (MUTEX_MAP_SIZE - 1)];
    auto& m = entry.versioned_mutex;
    uint64_t expected = m.load(std::memory_order_relaxed);
    if (expected == 0 || (expected >> PTR_BITS) != (g_cp_version & ((1 << (64 - PTR_BITS)) - 1))) {
        uint64_t desired = (g_cp_version << PTR_BITS) | (uint64_t)mutex;
        if (m.compare_exchange_strong(expected, desired, std::memory_order_acquire)) {
            return &entry.csite;
        }
    }
    return nullptr;
}

bool remove_pthread_contention_site(pthread_mutex_t* mutex, pthread_contention_site_t* saved_csite) {
    MutexMapEntry& entry = g_mutex_map[hash_mutex_ptr(mutex) & (MUTEX_MAP_SIZE - 1)];
    auto& m = entry.versioned_mutex;
    if ( (m.load(std::memory_order_relaxed) & ( ( ((uint64_t)1) << PTR_BITS) - 1))
        != (uint64_t)mutex) {
        return false;
    }
    *saved_csite = entry.csite;
    make_contention_site_invalid(&entry.csite);
    m.store(0, std::memory_order_release);
    return true;
}

// 注意这个函数在锁外执行
void submit_contention(const pthread_contention_site_t& csite, int64_t now_ns) {
    // 使用 TLS 进行加锁，收集锁竞争的代码中可能会调用 pthread_mutex_lock
    tls_inside_lock = true;
    // 从对象池中获取一个对象
    SampledContention* sc = get_object<SampledContention>();
    sc->duration_ns = csite.duration_ns * COLLECTOR_SAMPLING_BASE / csite.sampling_range;
    sc->count = COLLECTOR_SAMPLING_BASE / static_cast<double>(csite.sampling_range);
    sc->frames_count = backtrace(sc->stack, sizeof(sc->stack) / sizeof(sc->stack[0]));
    LOG(DEBUG) << "submit_contention: duration_ns: " << sc->duration_ns
        << ", count: " << sc->count << ", frames_count: " << sc->frames_count;
    sc->submit(now_ns / 1000);
    tls_inside_lock = false;
}

int pthread_mutex_lock_impl(pthread_mutex_t* mutex) {
    // 在 ld 链接加载的时候，有可能 constructor 还没有被调用，这里调用一次
    if (__glibc_unlikely(real_pthread_mutex_lock_func == nullptr)) {
        mutex_hook_init();
    }
    // 收集锁竞争信息的代码可能会调用 pthread_mutex_lock，并且可能会造成死锁，因此不采样
    if (!g_cp || tls_inside_lock) {
        return real_pthread_mutex_lock_func(mutex);
    }
    // 对于没有竞争的锁，直接放行，不要减慢人家的速度
    int res = pthread_mutex_trylock(mutex);
    if (res != EBUSY) {
        // EBUSY 表示 mutex 所指向的互斥锁已锁定，无法获取，有竞争
        return res;
    }
    LOG(DEBUG) << "start sampling";
    const size_t sampling_range = is_collectable(&g_cp_sl);

    pthread_contention_site_t* csite = nullptr;
    TLSPthreadContentionSites& fast_alt = tls_csites;
    if (fast_alt.cp_version != g_cp_version) {
        fast_alt.cp_version = g_cp_version;
        fast_alt.count = 0;
    }
    if (fast_alt.count < TLS_MAX_COUNT) {
        MutexAndContentionSite& entry = fast_alt.list[fast_alt.count++];
        entry.mutex = mutex;
        csite = &entry.csite;
        if (!sampling_range) {
            make_contention_site_invalid(&entry.csite);
            return real_pthread_mutex_lock_func(mutex);
        }
    }
    if (!sampling_range) {
        return real_pthread_mutex_lock_func(mutex);
    }
    const uint64_t start_time_ns = Util::get_monotonic_time_ns();
    res = real_pthread_mutex_lock_func(mutex);
    if (res == 0) {
        if (csite == nullptr) {
            csite = add_pthread_contention_site(mutex);
            if (csite == nullptr) {
                return res;
            }
        }
        csite->duration_ns = Util::get_monotonic_time_ns() - start_time_ns;
        csite->sampling_range = sampling_range;
    }
    return res;
}

int pthread_mutex_unlock_impl(pthread_mutex_t* mutex) {
    if (__glibc_unlikely(real_pthread_mutex_unlock_func == nullptr)) {
        mutex_hook_init();
    }
    if (!g_cp || tls_inside_lock) {
        return real_pthread_mutex_unlock_func(mutex);
    }
    uint64_t unlock_start_time_ns = 0;
    bool miss_in_tls = true;
    pthread_contention_site_t saved_csite = {0, 0};
    TLSPthreadContentionSites& fast_alt = tls_csites;
    for (int i = fast_alt.count - 1; i >= 0; --i) {
        if (fast_alt.list[i].mutex == mutex) {
            if (is_contention_site_valid(fast_alt.list[i].csite)) {
                saved_csite = fast_alt.list[i].csite;
                unlock_start_time_ns = Util::get_monotonic_time_ns();
            }
            fast_alt.list[i] = fast_alt.list[--fast_alt.count];
            miss_in_tls = false;
            break;
        }
    }
    if (miss_in_tls) {
        if (remove_pthread_contention_site(mutex, &saved_csite)) {
            unlock_start_time_ns = Util::get_monotonic_time_ns();
        }
    }
    int res = real_pthread_mutex_unlock_func(mutex);
    // 注意: 这里往下属于锁外
    if (unlock_start_time_ns) {
        uint64_t unlock_end_time_ns = Util::get_monotonic_time_ns();
        saved_csite.duration_ns += unlock_end_time_ns - unlock_start_time_ns;
        submit_contention(saved_csite, unlock_end_time_ns);
    }
    return res;
}

}  // namespace contention_prof
