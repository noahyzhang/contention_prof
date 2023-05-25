/**
 * @file profiler.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-28
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <string>
#include <fstream>
#include <unordered_set>
#include "collector.h"

namespace contention_prof {

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

extern ContentionProfiler* g_cp;
extern pthread_mutex_t g_cp_mutex;
extern uint64_t g_cp_version;

bool contention_profiler_start(const char* filename);
void contention_profiler_stop();

}  // namespace contention_prof
