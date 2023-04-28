#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "profiler.h"

int count = 0;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void* thr_func(void*) {
    pthread_mutex_lock(&mtx);
    count += 10;
    sleep(1);
    pthread_mutex_unlock(&mtx);
}

int main() {
    const char* filename = "./result.prof";
    bool res = contention_prof::contention_profiler_start(filename);
    if (!res) {
        std::cout << "contention profiler start failed" << std::endl;
        return -1;
    }

    // pthread_mutex_lock(&mtx);
    // count++;
    // pthread_mutex_unlock(&mtx);

    // pthread_mutex_destroy(&mtx);
    const int ARR_SIZE = 20;
    std::vector<pthread_t> thread_arr(ARR_SIZE, 0);
    for (int i = 0; i < ARR_SIZE; ++i) {
        pthread_create(&thread_arr[i], nullptr, thr_func, nullptr);
    }
    for (int i = 0; i < ARR_SIZE; ++i) {
        pthread_join(thread_arr[i], nullptr);
    }
    std::cout << "count: " << count << std::endl;

    contention_prof::contention_profiler_stop();
    return 0;
}
