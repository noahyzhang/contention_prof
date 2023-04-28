#include <math.h>
#include <memory>
#include "common/log.h"
#include "profiler.h"

namespace contention_prof {


ContentionProfiler* g_cp = nullptr;
pthread_mutex_t g_cp_mutex = PTHREAD_MUTEX_INITIALIZER;
uint64_t g_cp_version = 0;

const size_t MAX_CACHED_CONTENTIONS = 512;
const int SKIPPED_STACK_FRAMES = 2;

ContentionProfiler::ContentionProfiler(const char* name)
    : init_(false)
    , first_write_(true)
    , filename_(name) {}

ContentionProfiler::~ContentionProfiler() {
    if (!init_) {
        return;
    }
    flush_to_disk(true);
    file_stream_.close();
}

void ContentionProfiler::init_if_needed() {
    if (!init_) {
        try {
            std::remove(filename_.c_str());
            file_stream_.open(filename_, std::ofstream::out | std::ofstream::app);
        } catch(...) {
            // LOG(ERROR) << "";
            return;
        }
        file_stream_ << "--- contention\ncycles/second=10000000000\n";
        init_ = true;
    }
}

void ContentionProfiler::dump_and_destroy(SampledContention* c) {
    init_if_needed();
    auto iter = hash_set.find(c);
    if (iter != hash_set.end()) {
        (*iter)->duration_ns += c->duration_ns;
        (*iter)->count += c->count;
        c->destroy();
    } else {
        hash_set.insert(c);
    }
    if (hash_set.size() > MAX_CACHED_CONTENTIONS) {
        flush_to_disk(false);
    }
}

void ContentionProfiler::flush_to_disk(bool ending) {
    if (!hash_set.empty()) {
        for (const auto c : hash_set) {
            file_stream_ << c->duration_ns << ' ' << static_cast<size_t>(ceil(c->count)) << " @";
            for (int i = SKIPPED_STACK_FRAMES; i < c->frames_count; ++i) {
                file_stream_ << ' ' << reinterpret_cast<void*>(c->stack[i]);
            }
            file_stream_ << '\n';
            c->destroy();
        }
        hash_set.clear();
    }
    if (ending) {
        file_stream_ << Util::get_self_maps();
    }
}

bool contention_profiler_start(const char* filename) {
    if (filename == nullptr) {
        return false;
    }
    if (g_cp) {
        return false;
    }
    std::unique_ptr<ContentionProfiler> ctx(new ContentionProfiler(filename));
    {
        pthread_mutex_lock(&g_cp_mutex);
        if (g_cp) {
            pthread_mutex_unlock(&g_cp_mutex);
            return false;
        }
        g_cp = ctx.release();
        ++g_cp_version;
        pthread_mutex_unlock(&g_cp_mutex);
    }
    return true;
}

void contention_profiler_stop() {
    ContentionProfiler* ctx = nullptr;
    if (g_cp) {
        pthread_mutex_lock(&g_cp_mutex);
        if (g_cp) {
            ctx = g_cp;
            g_cp = nullptr;
            pthread_mutex_unlock(&g_cp_mutex);

            ctx->init_if_needed();
            delete ctx;
            return;
        }
        pthread_mutex_unlock(&g_cp_mutex);
    }
    LOG(ERROR) << "Contention profiler is not started!";
}

}  // namespace contention_prof
