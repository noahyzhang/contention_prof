#include <math.h>
#include <mutex>
#include <sstream>
#include "common/object_pool.h"
#include "sample.h"

namespace contention_prof {

const size_t MAX_CACHED_CONTENTIONS = 512;
const int SKIPPED_STACK_FRAMES = 2;

// ------------------- ContentionProfiler -------------------

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
            file_stream_.open(filename_, std::ofstream::out | std::ofstream::app);
        } catch(...) {
            LOG(ERROR) << "";
            return;
        }
        file_stream_ << "--- contention\ncycles/second=1000000000\n";
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


// ----------- SampledContention ------------------

static ContentionProfiler* g_cp = nullptr;
static std::mutex g_cp_mutex;

void SampledContention::dump_and_destroy(size_t) {
    if (g_cp) {
        std::lock_guard<std::mutex> guard(g_cp_mutex);
        if (g_cp) {
            g_cp->dump_and_destroy(this);
            return;
        }
    }
    destroy();
}

void SampledContention::destroy() {
    return_object(this);
}



}  // namespace contention_prof
