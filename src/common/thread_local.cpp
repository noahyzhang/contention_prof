#include <utility>
#include <algorithm>
#include <vector>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include "thread_local.h"

namespace contention_prof {

/**
 * @brief 线程退出工具类
 * 
 */
class ThreadExitHelper {
public:
    // 注册函数和参数
    using Fn = void(*)(void*);
    using Pair = std::pair<Fn, void*>;

    /**
     * @brief 析构时执行所有已经注册的函数
     * 
     */
    ~ThreadExitHelper() {
        for (; !fns_.empty();) {
            Pair back = fns_.back();
            fns_.pop_back();
            back.first(back.second);
        }
    }

    /**
     * @brief 注册一个函数
     * 
     * @param fn 
     * @param arg 
     * @return int 
     */
    int add(Fn fn, void* arg) {
        try {
            if (fns_.capacity() < 16) {
                fns_.reserve(16);
            }
            fns_.emplace_back(fn, arg);
        } catch(...) {
            errno = ENOMEM;
            return -1;
        }
        return 0;
    }

    /**
     * @brief 移除已注册的函数
     * 
     * @param fn 
     * @param arg 
     */
    void remove(Fn fn, void* arg) {
        auto start_iter = std::find(fns_.begin(), fns_.end(), std::make_pair(fn, arg));
        if (start_iter != fns_.end()) {
            auto end_iter = start_iter+1;
            for (; end_iter != fns_.end() && end_iter->first == fn && end_iter->second == arg;) {
                ++end_iter;
            }
            fns_.erase(start_iter, end_iter);
        }
    }

private:
    std::vector<Pair> fns_;
};

static pthread_key_t thread_atexit_key;
static pthread_once_t thread_atexit_once = PTHREAD_ONCE_INIT;

static void delete_thread_exit_helper(void* arg) {
    delete static_cast<ThreadExitHelper*>(arg);
}

static void helper_exit_global() {
    ThreadExitHelper* h = reinterpret_cast<ThreadExitHelper*>(pthread_getspecific(thread_atexit_key));
    if (h != nullptr) {
        pthread_setspecific(thread_atexit_key, nullptr);
        delete h;
    }
}

/**
 * @brief 设置线程退出时的操作
 * 
 */
static void make_thread_atexit_key() {
    if (pthread_key_create(&thread_atexit_key, delete_thread_exit_helper) != 0) {
        fprintf(stderr, "Fail to create thread_atexit_key, abort\n");
        abort();
    }
    // 在 exit 的时候注册调用
    atexit(helper_exit_global);
}

/**
 * @brief 获取或者创建一个线程退出工具对象
 * 
 * @return ThreadExitHelper* 
 */
ThreadExitHelper* get_or_new_thread_exit_helper() {
    pthread_once(&thread_atexit_once, make_thread_atexit_key);
    ThreadExitHelper* h = reinterpret_cast<ThreadExitHelper*>(pthread_getspecific(thread_atexit_key));
    if (h == nullptr) {
        h = new ThreadExitHelper();
        if (h != nullptr) {
            pthread_setspecific(thread_atexit_key, h);
        }
    }
    return h;
}

/**
 * @brief 获取线程局部缓存中的线程退出工具对象
 * 
 * @return ThreadExitHelper* 
 */
ThreadExitHelper* get_thread_exit_helper() {
    pthread_once(&thread_atexit_once, make_thread_atexit_key);
    return reinterpret_cast<ThreadExitHelper*>(pthread_getspecific(thread_atexit_key));
}

/**
 * @brief 执行函数（无参）
 * 
 * @param fn 
 */
static void call_single_arg_fn(void* fn) {
    using Fn = void(*)();
    (reinterpret_cast<Fn>(fn))();
}

/**
 * @brief 注册一个带参函数在线程退出时调用
 * 
 * @param fn 
 * @param arg 
 * @return int 
 */
int thread_atexit(void (*fn)(void*), void* arg) {
    if (fn == nullptr) {
        errno = EINVAL;
        return -1;
    }
    ThreadExitHelper* h = get_or_new_thread_exit_helper();
    if (h != nullptr) {
        return h->add(fn, arg);
    }
    errno = ENOMEM;
    return -1;
}

/**
 * @brief 注册一个无参函数在线程退出时调用
 * 
 * @param fn 
 * @return int 
 */
int thread_atexit(void (*fn)()) {
    if (fn == nullptr) {
        errno = EINVAL;
        return -1;
    }
    return thread_atexit(call_single_arg_fn, reinterpret_cast<void*>(fn));
}

void thread_atexit_cancel(void (*fn)(void*), void* arg) {
    if (fn != nullptr) {
        ThreadExitHelper* h = get_thread_exit_helper();
        if (h) {
            h->remove(fn, arg);
        }
    }
}

/**
 * @brief 移除一个无参函数
 * 
 * @param fn 
 */
void thread_atexit_cancel(void (*fn)()) {
    if (fn != nullptr) {
        thread_atexit_cancel(call_single_arg_fn, reinterpret_cast<void*>(fn));
    }
}


}  // namespace contention_prof
