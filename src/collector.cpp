#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <map>
#include <mutex>
#include <list>
#include <gflags/gflags.h>
#include "common/time.h"
#include "common/log.h"
#include "collector.h"

namespace contention_prof {

DEFINE_int32(collector_max_pending_samples, 1000, "Destroy unprocessed samples when they're too many");
DEFINE_int32(collector_expected_per_second, 1000, "Expected number of samples to be collected per second");

static CollectorSpeedLimit g_null_speed_limit;

struct CombineCollected {
    void operator()(Collected*& s1, Collected* s2) const {
        if (s2 == nullptr) {
            return;
        }
        if (s1 == nullptr) {
            s1 = s2;
            return;
        }
        s1->insert_before_as_list(s2);
    }
};

class Collector : public Reducer<Collected*, CombineCollected> {
public:
    static Collector* get_instance() {
        static Collector instance;
        return &instance;
    }
    ~Collector();
    Collector(const Collector&) = delete;
    Collector& operator=(const Collector&) = delete;
    Collector(Collector&&) = delete;
    Collector& operator=(Collector&&) = delete;

private:
    Collector();

public:
    int64_t last_active_cpuwide_us() const {
        return last_active_cpuwide_us_;
    }

    void wakeup_grab_thread();

private:
    void grab_thread();
    void dump_thread();

    void update_speed_limit(
        CollectorSpeedLimit* speed_limit,
        size_t* last_grab_count,
        size_t cur_grab_count,
        int64_t interval_us);

    static void* run_grab_thread(void* arg) {
        static_cast<Collector*>(arg)->grab_thread();
        return nullptr;
    }
    static void* run_dump_thread(void* arg) {
        static_cast<Collector*>(arg)->dump_thread();
        return nullptr;
    }

private:
    int64_t last_active_cpuwide_us_{0};
    bool created_{false};
    bool stop_{false};
    pthread_t grab_thread_{0};
    pthread_t dump_thread_{0};
    int64_t grab_count_{0};
    int64_t drop_count_{0};
    int64_t dump_count_{0};
    pthread_mutex_t dump_thread_mutex_;
    pthread_cond_t dump_thread_cond_;
    LinkNode<Collected> dump_root_;
    pthread_mutex_t sleep_mutex_;
    pthread_cond_t sleep_cond_;
};

Collector::Collector() {
    last_active_cpuwide_us_ = Util::get_monotonic_time_us();
    pthread_mutex_init(&dump_thread_mutex_, nullptr);
    pthread_cond_init(&dump_thread_cond_, nullptr);
    pthread_mutex_init(&sleep_mutex_, nullptr);
    pthread_cond_init(&sleep_cond_, nullptr);
    int res = pthread_create(&grab_thread_, nullptr, run_grab_thread, this);
    if (res != 0) {
        LOG(ERROR) << "Fail to create Collector, " << strerror(errno);
    } else {
        created_ = true;
    }
}

Collector::~Collector() {
    if (created_) {
        stop_ = true;
        pthread_join(grab_thread_, nullptr);
        created_ = false;
    }
    pthread_mutex_destroy(&dump_thread_mutex_);
    pthread_cond_destroy(&dump_thread_cond_);
    pthread_mutex_destroy(&sleep_mutex_);
    pthread_cond_destroy(&sleep_cond_);
}

void Collector::grab_thread() {
    last_active_cpuwide_us_ = Util::get_monotonic_time_us();
    int64_t last_before_update_sl = last_active_cpuwide_us_;

    int res = pthread_create(&dump_thread_, nullptr, run_dump_thread, this);
    if (res < 0) {
        LOG(ERROR) << "pthread_create failed, err: " << strerror(errno);
        return;
    }

    using GrabMap = std::map<CollectorSpeedLimit*, size_t>;
    GrabMap last_grab_count_map;
    GrabMap grab_count_map;
    using PreprocessorMap = std::map<CollectorPreprocessor*, std::vector<Collected*>>;
    PreprocessorMap prep_map;

    for (; !stop_;) {
        const int64_t abstime = last_active_cpuwide_us_ + COLLECTOR_GRAB_INTERVAL_US;
        for (auto it = prep_map.begin(); it != prep_map.end(); ++it) {
            it->second.clear();
        }
        // 获取到所有的 Agent（存储数据的链表）
        LinkNode<Collected>* head = this->reset();
        if (head) {
            // 让 tmp_root 指向这个 head
            LinkNode<Collected> tmp_root;
            head->insert_before_as_list(&tmp_root);
            head = nullptr;

            // 将所有 Agent 中的数据归类到 prep_map 中
            for (LinkNode<Collected>* p = tmp_root.next(); p != &tmp_root;) {
                LinkNode<Collected>* saved_next = p->next();
                // 先把 p 从链表中移除
                p->remove_from_list();
                // 按照预处理后的结果将其归类
                CollectorPreprocessor* prep = p->value()->preprocessor();
                prep_map[prep].push_back(p->value());
                p = saved_next;
            }
            LinkNode<Collected> root;
            for (PreprocessorMap::iterator it = prep_map.begin(); it != prep_map.end(); ++it) {
                std::vector<Collected*>& list = it->second;
                if (it->second.empty()) {
                    continue;
                }
                // 将分类后的每条链表进行处理
                if (it->first != nullptr) {
                    it->first->process(list);
                }
                // 对链表中每个数据进行处理
                for (size_t i = 0; i < list.size(); ++i) {
                    Collected* p = list[i];
                    // 对于不同 speed_limit 做分类
                    CollectorSpeedLimit* speed_limit = p->speed_limit();
                    if (speed_limit == nullptr) {
                        ++grab_count_map[&g_null_speed_limit];
                    } else {
                        ++grab_count_map[speed_limit];
                    }
                    ++grab_count_;
                    // 在做一次筛选
                    if (grab_count_ >= drop_count_ + dump_count_ + FLAGS_collector_max_pending_samples) {
                        ++drop_count_;
                        p->destroy();
                    } else {
                        p->insert_before(&root);
                    }
                }
            }
            // 将最终的链表赋给 dump_root_，并且唤醒 dump_thread 线程处理
            if (root.next() != &root) {
                LinkNode<Collected>* head2 = root.next();
                root.remove_from_list();
                pthread_mutex_lock(&dump_thread_mutex_);
                head2->insert_before_as_list(&dump_root_);
                pthread_cond_signal(&dump_thread_cond_);
                pthread_mutex_unlock(&dump_thread_mutex_);
            }
        }
        int64_t now = Util::get_monotonic_time_us();
        // grab_thread 线程处理一次 “所有 agent 的数据” 花费的时间
        int64_t interval = now - last_before_update_sl;
        last_before_update_sl = now;
        // 更新每一种 speed_limit 的信息
        for (GrabMap::iterator it = grab_count_map.begin(); it != grab_count_map.end(); ++it) {
            update_speed_limit(it->first, &last_grab_count_map[it->first], it->second, interval);
        }
        // 默认 COLLECTOR_GRAB_INTERVAL_US（100ms）的轮循处理时间，如果处理时间小于 100ms，那就睡眠凑够 100ms
        now = Util::get_monotonic_time_us();
        last_active_cpuwide_us_ = now;
        if (!stop_ && abstime > now) {
            timespec abstimespec = microseconds_from_now(abstime - now);
            pthread_mutex_lock(&sleep_mutex_);
            pthread_cond_timedwait(&sleep_cond_, &sleep_mutex_, &abstimespec);
            pthread_mutex_unlock(&sleep_mutex_);
        }
        last_active_cpuwide_us_ = Util::get_monotonic_time_us();
    }
    pthread_mutex_lock(&dump_thread_mutex_);
    stop_ = true;
    pthread_cond_signal(&dump_thread_cond_);
    pthread_mutex_unlock(&dump_thread_mutex_);
    pthread_join(dump_thread_, nullptr);
}

void Collector::wakeup_grab_thread() {
    pthread_mutex_lock(&sleep_mutex_);
    pthread_cond_signal(&sleep_cond_);
    pthread_mutex_unlock(&sleep_mutex_);
}

void Collector::update_speed_limit(
    CollectorSpeedLimit* sl, size_t* last_grab_count,
    size_t cur_grab_count, int64_t interval_us) {
    const size_t round_grab_count = cur_grab_count - *last_grab_count;
    if (round_grab_count == 0) {
        return;
    }
    *last_grab_count = cur_grab_count;
    if (interval_us < 0) {
        interval_us = 0;
    }
    size_t new_sampling_range = 0;
    const size_t old_sampling_range = sl->sampling_range;
    if (!sl->ever_grabbed) {
        if (sl->first_sample_real_us) {
            interval_us = Util::gettimeofday_us() - sl->first_sample_real_us;
            if (interval_us < 0) {
                interval_us = 0;
            }
        } else {

        }
        new_sampling_range = FLAGS_collector_expected_per_second
            * interval_us * COLLECTOR_SAMPLING_BASE / (1000000L * round_grab_count);
    } else {
        new_sampling_range = FLAGS_collector_expected_per_second
            * interval_us * old_sampling_range / (1000000L * round_grab_count);
        if (interval_us < 1000000L) {
            new_sampling_range = (new_sampling_range * interval_us
                + old_sampling_range * (1000000L - interval_us)) / 1000000L;
        }
    }
    // 处理 new_sampling_range 边界值
    if (new_sampling_range == 0) {
        new_sampling_range = 1;
    } else if (new_sampling_range > COLLECTOR_SAMPLING_BASE) {
        new_sampling_range = COLLECTOR_SAMPLING_BASE;
    }
    if (new_sampling_range != old_sampling_range) {
        sl->sampling_range = new_sampling_range;
    }
    // 打开 ever_grabbed 为 true，当判断是否收集时，采用随机数方式
    if (!sl->ever_grabbed) {
        sl->ever_grabbed = true;
    }
}

void Collector::dump_thread() {
    // int64_t last_ns = Util::get_monotonic_time_ns();
    // double busy_seconds = 0;

    LinkNode<Collected> root;
    size_t round = 0;
    for (; !stop_;) {
        ++round;
        LinkNode<Collected>* new_head = nullptr;
        {
            pthread_mutex_lock(&dump_thread_mutex_);
            // 如果 dump_root_ 为空，则等待
            for (; !stop_ && dump_root_.next() == &dump_root_;) {
                // const uint64_t now_ns = Util::get_monotonic_time_ns();
                // busy_seconds += (now_ns - last_ns) / 1E9;
                pthread_cond_wait(&dump_thread_cond_, &dump_thread_mutex_);
                // last_ns = Util::get_monotonic_time_ns();
            }
            if (stop_) {
                break;
            }
            new_head = dump_root_.next();
            dump_root_.remove_from_list();
            pthread_mutex_unlock(&dump_thread_mutex_);
        }
        new_head->insert_before_as_list(&root);
        // 处理这些数据，对每个数据调用 dump_and_destroy 处理
        for (LinkNode<Collected>* p = root.next(); !stop_ && p != &root;) {
            LinkNode<Collected>* saved_next = p->next();
            p->remove_from_list();
            Collected* s = p->value();
            s->dump_and_destroy(round);
            ++dump_count_;
            p = saved_next;
        }
    }
}

size_t is_collectable_before_first_time_grabbed(CollectorSpeedLimit* sl) {
    if (!sl->ever_grabbed) {
        int before_add = sl->count_before_grabbed.fetch_add(1, std::memory_order_relaxed);
        if (before_add == 0) {
            sl->first_sample_real_us = Util::get_monotonic_time_us();
        } else if (before_add >= FLAGS_collector_expected_per_second) {
            // 如果 grab_thread 线程在执行前，积攒的数量已经超过了 FLAGS_collector_expected_per_second
            // 那就触发 grab_thread 线程去处理
            Collector::get_instance()->wakeup_grab_thread();
        }
    }
    return sl->sampling_range;
}

inline size_t is_collectable(CollectorSpeedLimit* speed_limit) {
    if (__glibc_likely(speed_limit->ever_grabbed)) {
        const size_t sampling_range = speed_limit->sampling_range;
        if ((fast_rand() & (COLLECTOR_SAMPLING_BASE - 1)) >= sampling_range) {
            return 0;
        }
        return sampling_range;
    }
    return is_collectable_before_first_time_grabbed(speed_limit);
}

void Collected::submit(uint64_t cpu_time_us) {
    Collector* d = Collector::get_instance();
    // last_active_cpuwide_us() 会被 grab_thread 线程周期性的更新
    // 对于周期外的数据，就地销毁。因为我们期望 grab_thread 线程可以 200ms 完成一次刷新
    // 超过 200ms 也就同时说明数据过多，处理太慢了，就地销毁没有符合逻辑
    if (cpu_time_us < d->last_active_cpuwide_us() + COLLECTOR_GRAB_INTERVAL_US * 2) {
        *d << this;
    } else {
        destroy();
    }
}

}  // namespace contention_prof
