/**
 * @file contention.h
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

namespace contention_prof {

typedef struct {
    int64_t duration_ns;
    size_t sampling_range;
} pthread_contention_site_t;

void mutex_hook_init();

int pthread_mutex_lock_impl(pthread_mutex_t* mutex);
int pthread_mutex_unlock_impl(pthread_mutex_t* mutex);
 
}  // namespace contention_prof
