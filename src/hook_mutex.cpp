#include "contention.h"
#include "hook_mutex.h"

using contention_prof::mutex_hook_init;
using contention_prof::pthread_mutex_lock_impl;
using contention_prof::pthread_mutex_unlock_impl;

// 系统自动调用
__attribute__((constructor)) static void mutex_hook_constructor() {
    mutex_hook_init();
}

// 重写 pthread_mutex_lock 系统调用
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    return pthread_mutex_lock_impl(mutex);
}

// 重写 pthread_mutex_unlock 系统调用
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return pthread_mutex_unlock_impl(mutex);
}