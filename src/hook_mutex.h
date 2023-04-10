/**
 * @file hook_mutex.h
 * @author noahyzhang
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int pthread_mutex_lock(pthread_mutex_t *mutex)__THROWNL __nonnull((1));
extern int pthread_mutex_unlock(pthread_mutex_t *mutex)__THROWNL __nonnull((1));

#ifdef __cplusplus
}
#endif
