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
#include <gflags/gflags.h>
#include "common/common.h"
#include "common/linked_list.h"
#include "common/reducer.h"

namespace contention_prof {

DEFINE_int32(collector_expected_per_second, 1000, "Expected number of samples to be collected per second");

struct CollectorSpeedLimit {
    size_t sampling_range;
    bool ever_grabbed;
    std::atomic<int> count_before_grabbed;
    int64_t first_sample_real_us;
};

static const size_t COLLECTOR_SAMPLING_BASE = 16384;

class Collected;
class CollectorPreprocessor {
public:
    virtual void process(std::vector<Collected*>& sample) = 0;
};

class Collected : public LinkNode<Collected> {
public:
    void submit(uint64_t time_us);
    virtual void dump_and_destroy(size_t round_index) = 0;
    virtual void destroy() = 0;
    virtual CollectorSpeedLimit* speed_limit() = 0;
    virtual CollectorPreprocessor* preprocessor() { return nullptr; }
};

size_t is_collectable_before_first_time_grabbed(CollectorSpeedLimit* sl) {
    if (!sl->ever_grabbed) {
        int before_add = sl->count_before_grabbed.fetch_add(1, std::memory_order_relaxed);
        if (before_add == 0) {
            sl->first_sample_real_us = Util::get_monotonic_time_us();
        } else if (before_add >= FLAGS_collector_expected_per_second) {
            Collector::get_instance()->wakeup_grab_thread();
        }
    }
    return sl->sampling_range;
}

inline size_t is_collectable(CollectorSpeedLimit* speed_limit) {
    if (__glibc_likely(speed_limit->ever_grabbed)) {
        const size_t sampling_range = speed_limit->sampling_range;
        if ((Util::fast_rand() & (COLLECTOR_SAMPLING_BASE - 1)) >= sampling_range) {
            return 0;
        }
        return sampling_range;
    }
    is_collectable_before_first_time_grabbed(speed_limit);
}

}  // namespace contention_prof
