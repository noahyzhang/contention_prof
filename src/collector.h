/**
 * @file collector.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <atomic>

namespace contention_prof {

struct CollectorSpeedLimit {
    size_t sampling_range;
    bool ever_grabbed;
    std::atomic<int> count_before_grabbed;
    int64_t first_sample_real_us;
};

class Collected;
class CollectorPreprocessor {
public:
    virtual void process(std::vector<Collected*>& sample) = 0;
};

class Collected {
public:
    void submit(uint64_t time_us);
    virtual void dump_and_destroy(size_t round_index) = 0;
    virtual void destroy() = 0;
    virtual CollectorSpeedLimit* speed_limit() = 0;
    virtual CollectorPreprocessor* preprocessor() { return nullptr; }
};

struct SampleContention : public Collected {
    int64_t duration_ns;
    double count;
    int nframes;
    void* stack[26];

    void dump_and_destroy(size_t round);
    void destroy();
    CollectorSpeedLimit* speed_limit() {
        return &g_cp_sl;
    }
    size_t hash_code() const {
        if (nframes == 0) {
            return 0;
        }
        uint32_t code = 1;
        uint32_t seed = nframes;
        // ???
        return code;
    }
}

}  // namespace contention_prof
