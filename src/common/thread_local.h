/**
 * @file thread_local.h
 * @author noahyzhang
 * @brief 
 * @version 0.1
 * @date 2023-04-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

namespace contention_prof {

template <typename T>
inline T* get_thread_local();

int thread_atexit(void (*fn)());
int thread_atexit(void (*fn)(void*), void* arg);

void thread_atexit_cancel(void (*fn)());
void thread_atexit_cancel(void (*fn)(void*), void* arg);

template <typename T>
void delete_object(void* arg) {
    delete static_cast<T*>(arg);
}

}  // namespace contention_prof

#include "thread_local_inl.h"
