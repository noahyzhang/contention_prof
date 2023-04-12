/**
 * @file fast_rand.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <stdint.h>

namespace contention_prof {

struct FastRandSeed {
    uint64_t s[2];
};

void init_fast_rand_seed(FastRandSeed* seed);

uint64_t fast_rand();
uint64_t fast_rand(FastRandSeed*);

uint64_t fast_rand_less_than(uint64_t range);

template <typename T>
T fast_rand_in(T min, T max) {
    extern int64_t fast_rand_in_64(int64_t min, int64_t max);
    extern uint64_t fast_rand_in_u64(uint64_t min, uint64_t max);
    if ((T)-1 < 0) {
        return fast_rand_in_64((int64_t)min, (int64_t)max);
    } else {
        return fast_rand_in_u64((uint64_t)min, (uint64_t)max);
    }
}

}  // namespace contention_prof
