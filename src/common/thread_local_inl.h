/**
 * @file thread_local_inl.h
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
class ThreadLocalHelper {
public:
    inline static T* get() {
        if (__glibc_likely(value != nullptr)) {
            return value;
        }
        value = new T();
        if (value != nullptr) {
            thread_atexit(delete_object<T>, value);
        }
        return value;
    }

private:
    static __thread T* value;
};

template <typename T>
__thread T* ThreadLocalHelper<T>::value = nullptr;

template <typename T>
inline T* get_thread_local() {
    return ThreadLocalHelper<T>::get();
}

}  // namespace contention_prof
