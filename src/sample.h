/**
 * @file sample.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <fstream>
#include <unordered_set>
#include "collector.h"

namespace contention_prof {

static CollectorSpeedLimit g_cp_sl;

struct SampledContention : public Collected {
    int64_t duration_ns;
    double count;
    int frames_count;
    void* stack[26];

    void dump_and_destroy(size_t round);
    void destroy();
    CollectorSpeedLimit* speed_limit() {
        return &g_cp_sl;
    }

    size_t hash_code() const {
        if (frames_count == 0) {
            return 0;
        }
        uint32_t code = 1;
        uint32_t seed = frames_count;
        Util::MurmurHash3_x86_32(stack, sizeof(void*) * frames_count, seed, &code);
        return code;
    }
};

class ContentionProfiler {
public:
    explicit ContentionProfiler(const char* name);
    ~ContentionProfiler();
    void dump_and_destroy(SampledContention* c);
    void flush_to_disk(bool ending);
    void init_if_needed();
private:
    bool init_;
    bool first_write_;
    std::string filename_;
    std::ofstream file_stream_;
    std::unordered_set<SampledContention*> hash_set;
};

}  // namespace contention_prof
