/**
 * @file object_pool.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

namespace contention_prof {

// 内存块的最大字节数
template <typename T>
struct ObjectPoolBlockMaxSize {
    static const size_t value = 64 * 1024;
};

// 内存块最大的数量
template <typename T>
struct ObjectPoolBlockMaxItem {
    static const size_t value = 256;
};

template <typename T>
struct ObjectPoolFreeChunkMaxItem {
    static size_t value() { return 256; }
};

template <typename T>
struct ObjectPoolValidator {
    static bool validate(const T*) { return true; }
};

}  // namespace contention_prof

#include "object_pool_inl.h"

namespace contention_prof {

template <typename T>
inline T* get_object() {
    return ObjectPool<T>::get_instance()->get_object();
}

template <typename T, typename A1>
inline T* get_object(const A1& arg1) {
    return ObjectPool<T>::get_instance()->get_object(arg1);
}

template <typename T, typename A1, typename A2>
inline T* get_object(const A1& arg1, const A2& arg2) {
    return ObjectPool<T>::get_instance()->get_object(arg1, arg2);
}

template <typename T>
inline int return_object(T* ptr) {
    return ObjectPool<T>::get_instance()->return_object(ptr);
}

template <typename T>
inline void clear_objects() {
    ObjectPool<T>::get_instance()->clear_objects();
}

template <typename T>
ObjectPoolInfo describe_objects() {
    return ObjectPool<T>::get_instance()->describe_objects();
}

}  // namespace contention_prof
