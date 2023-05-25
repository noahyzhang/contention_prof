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
#include <gflags/gflags.h>
#include <vector>
#include <atomic>
#include "common/fast_rand.h"
#include "common/common.h"
#include "common/linked_list.h"
#include "common/reducer.h"
#include "common/murmurhash3.h"
#include "profiler.h"
#include "common/object_pool.h"

namespace contention_prof {

const size_t COLLECTOR_SAMPLING_BASE = 16384;
const int64_t COLLECTOR_GRAB_INTERVAL_US = 100000L;  // 100ms

struct CollectorSpeedLimit {
    size_t sampling_range;
    bool ever_grabbed;
    std::atomic<int> count_before_grabbed;
    int64_t first_sample_real_us;

    CollectorSpeedLimit()
        : sampling_range(COLLECTOR_SAMPLING_BASE)
        , ever_grabbed(false)
        , count_before_grabbed(0)
        , first_sample_real_us(0) {}
};

class Collected;
class CollectorPreprocessor {
public:
    virtual void process(std::vector<Collected*>& sample) = 0;
};

/**
 * @brief 存储数据基类
 * 
 */
class Collected : public LinkNode<Collected> {
public:
    void submit(uint64_t cpu_time_us);
    virtual void dump_and_destroy(size_t round_index) = 0;
    virtual void destroy() = 0;
    virtual CollectorSpeedLimit* speed_limit() = 0;
    virtual CollectorPreprocessor* preprocessor() { return nullptr; }
};

static CollectorSpeedLimit g_cp_sl;

/**
 * @brief 实际被存储的数据
 * 
 */
struct SampledContention : public Collected {
    int64_t duration_ns;
    double count;
    int frames_count;
    void* stack[26];

    /**
     * @brief 数据的拷贝和清理
     * 
     * @param round 
     */
    void dump_and_destroy(size_t round) {
        if (g_cp) {
            pthread_mutex_lock(&g_cp_mutex);
            if (g_cp) {
                g_cp->dump_and_destroy(this);
                pthread_mutex_unlock(&g_cp_mutex);
                return;
            }
            pthread_mutex_unlock(&g_cp_mutex);
        }
        destroy();
    }

    void destroy() {
        return_object(this);
    }

    CollectorSpeedLimit* speed_limit() {
        return &g_cp_sl;
    }

    size_t hash_code() const {
        if (frames_count == 0) {
            return 0;
        }
        uint32_t code = 1;
        uint32_t seed = frames_count;
        MurmurHash3_x86_32(stack, sizeof(void*) * frames_count, seed, &code);
        return code;
    }
};

inline size_t is_collectable(CollectorSpeedLimit* speed_limit);

}  // namespace contention_prof
