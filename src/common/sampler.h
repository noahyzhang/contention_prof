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

#include <time.h>
#include <mutex>
#include <vector>
#include <thread>
#include "common/type_traits.h"
#include "common/bounded_queue.h"
#include "common/linked_list.h"
#include "common/common.h"

namespace contention_prof {

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
    Sampler();
    virtual void take_sample() = 0;
    void schedule();
    void destroy();

protected:
    virtual ~Sampler();

    friend class SamplerCollector;
    bool used_;
    std::mutex mtx_;
};

struct VoidOp {
    template <typename T>
    T operator()(const T&, const T&) const {
        abort();
    }
};

template <typename R, typename T, typename Op, typename InvOp>
class ReducerSampler : public Sampler {
public:
    static const time_t MAX_SECONDS_LIMIT = 3600;

    explicit ReducerSampler(R* reducer)
        : _reducer(reducer)
        , _window_size(1) {
        
        // Invoked take_sample at begining so the value of the first second
        // would not be ignored
        take_sample();
    }
    ~ReducerSampler() {}

    void take_sample() {
        // Make _q ready.
        // If _window_size is larger than what _q can hold, e.g. a larger
        // Window<> is created after running of sampler, make _q larger.
        if ((size_t)_window_size + 1 > _q.capacity()) {
            const size_t new_cap =
                std::max(_q.capacity() * 2, (size_t)_window_size + 1);
            const size_t memsize = sizeof(Sample<T>) * new_cap;
            void* mem = malloc(memsize);
            if (NULL == mem) {
                return;
            }
            BoundedQueue<Sample<T> > new_q(
                mem, memsize, OWNS_STORAGE);
            Sample<T> tmp;
            while (_q.pop(&tmp)) {
                new_q.push(tmp);
            }
            new_q.swap(_q);
        }

        Sample<T> latest;
        if (is_same<InvOp, VoidOp>::value) {
            // The operator can't be inversed.
            // We reset the reducer and save the result as a sample.
            // Suming up samples gives the result within a window.
            // In this case, get_value() of _reducer gives wrong answer and
            // should not be called.
            latest.data = _reducer->reset();
        } else {
            // The operator can be inversed.
            // We save the result as a sample.
            // Inversed operation between latest and oldest sample within a
            // window gives result.
            // get_value() of _reducer can still be called.
            latest.data = _reducer->get_value();
        }
        latest.time_us = Util::gettimeofday_us();
        _q.elim_push(latest);
    }

    bool get_value(time_t window_size, Sample<T>* result) {
        if (window_size <= 0) {
            // LOG(FATAL) << "Invalid window_size=" << window_size;
            return false;
        }
        std::lock_guard<std::mutex> guard(mtx_);
        if (_q.size() <= 1UL) {
            // We need more samples to get reasonable result.
            return false;
        }
        Sample<T>* oldest = _q.bottom(window_size);
        if (NULL == oldest) {
            oldest = _q.top();
        }
        Sample<T>* latest = _q.bottom();
        // DCHECK(latest != oldest);
        if (is_same<InvOp, VoidOp>::value) {
            // No inverse op. Sum up all samples within the window.
            result->data = latest->data;
            for (int i = 1; true; ++i) {
                Sample<T>* e = _q.bottom(i);
                if (e == oldest) {
                    break;
                }
                _reducer->op()(result->data, e->data);
            }
        } else {
            // Diff the latest and oldest sample within the window.
            result->data = latest->data;
            _reducer->inv_op()(result->data, oldest->data);
        }
        result->time_us = latest->time_us - oldest->time_us;
        return true;
    }

    // Change the time window which can only go larger.
    int set_window_size(time_t window_size) {
        if (window_size <= 0 || window_size > MAX_SECONDS_LIMIT) {
            // LOG(ERROR) << "Invalid window_size=" << window_size;
            return -1;
        }
        std::lock_guard<std::mutex> guard(mtx_);
        // BAIDU_SCOPED_LOCK(_mutex);
        if (window_size > _window_size) {
            _window_size = window_size;
        }
        return 0;
    }

    void get_samples(std::vector<T> *samples, time_t window_size) {
        if (window_size <= 0) {
            // LOG(FATAL) << "Invalid window_size=" << window_size;
            return;
        }
        // BAIDU_SCOPED_LOCK(_mutex);
        std::lock_guard<std::mutex> guard(mtx_);
        if (_q.size() <= 1) {
            // We need more samples to get reasonable result.
            return;
        }
        Sample<T>* oldest = _q.bottom(window_size);
        if (NULL == oldest) {
            oldest = _q.top();
        }
        for (int i = 1; true; ++i) {
            Sample<T>* e = _q.bottom(i);
            if (e == oldest) {
                break;
            }
            samples->push_back(e->data);
        }
    }

private:
    R* _reducer;
    time_t _window_size;
    BoundedQueue<Sample<T> > _q;
};

}  // namespace contention_prof
