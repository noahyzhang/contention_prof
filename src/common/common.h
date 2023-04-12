/**
 * @file common.h
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
#include <string>

namespace contention_prof {

class Util {
public:
    static uint64_t get_monotonic_time_ns();
    static uint64_t get_monotonic_time_us();
    static int64_t gettimeofday_us();

    static uint64_t fmix64(uint64_t m);

    static std::string get_self_maps();
};

}  // namespace contention_prof
