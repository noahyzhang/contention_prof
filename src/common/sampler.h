/**
 * @file sampler.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <mutex>
#include "common/linked_list.h"

namespace contention_prof {

struct VoidOp {
    template <typename T>
    T operator()(const T&, const T&) const {
        abort();
    }
};

template <typename T>
struct Sample {
    T data;
    int64_t time_us;

    Sample() : data(), time_us(0) {}
    Sample(const T& data2, int64_t time2) : data(data2), time_us(time2) {}
};

class SamplerCollector;
class Sampler : public LinkNode<Sampler> {
public:
    Sampler() : used_(true) {}
    virtual void take_sample() = 0;
    void schedule();
    void destroy();

protected:
    virtual ~Sampler();

    friend class SamplerCollector;
    bool used_;
    std::mutex mtx_;
};

}  // namespace contention_prof
