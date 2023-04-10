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

namespace noahyzhang {
namespace contention_prof {

#define LOG(level)

class Util {
public:
    static uint64_t get_monotonic_time_ns();
    static uint64_t get_monotonic_time_us();
};

}  // contention_prof
}  // noahyzhang
