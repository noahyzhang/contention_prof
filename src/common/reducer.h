/**
 * @file reducer.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <string>
#include "common/sampler.h"
#include "common/series.h"
#include "common/combiner.h"
#include "common/type_traits.h"

namespace contention_prof {

template <typename T, typename Op, typename InvOp = VoidOp>
class Reducer {
public:
    using combiner_type = AgentCombiner<T, T, Op>;
    using agent_type = typename combiner_type::Agent;
    using sampler_type = ReducerSampler<Reducer, T, Op, InvOp>;

    class SeriesSampler : public Sampler {
    public:
        SeriesSampler(Reducer* owner, const Op& op)
            : owner_(owner), series_(op) {}
        ~SeriesSampler() {}

        void take_sample() {
            series_.append(owner_->get_value());
        }
        void describe(std::ostream& os) {
            series_.describe(os, nullptr);
        }

    private:
        Reducer* owner_;
        Series<T, Op> series_;
    };

public:
    Reducer(const T& identify = T(),
        const Op& op = Op(),
        const InvOp& inv_op = InvOp())
        : combiner_(identify, identify, op)
        , sampler_(nullptr)
        , series_sampler_(nullptr)
        , inv_op_(inv_op) {}

    ~Reducer() {
        hide();
        if (sampler_) {
            sampler_->destroy();
            sampler_ = nullptr;
        }
        if (series_sampler_) {
            series_sampler_->destroy();
            series_sampler_ = nullptr;
        }
    }

    Reducer& operator<<(const T& value);

    T get_value() const {
        return combiner_.combine_agents();
    }

    T reset() {
        return combiner_.reset_all_agents();
    }

    void describe(std::ostream& os, bool quote_string) const {
        if (is_same<T, std::string>::value && quote_string) {
            os << '"' << get_value() << '"';
        } else {
            os << get_value();
        }
    }

    bool valid() const {
        return combiner_.valid();
    }

    const Op& op() const {
        return combiner_.op();
    }

    const InvOp& inv_op() const {
        return inv_op_;
    }

    sampler_type* get_sampler() {
        if (sampler_ == nullptr) {
            sampler_ = new sampler_type(this);
            sampler_->schedule();
        }
        return sampler_;
    }

    int describe_series(std::ostream& os, const SeriesOptions& options) const {
        if (series_sampler_ == nullptr) {
            return 1;
        }
        if (!options.test_only) {
            series_sampler_->describe(os);
        }
        return 0;
    }

private:
    combiner_type combiner_;
    sampler_type* sampler_;
    SeriesSampler* series_sampler_;
    InvOp inv_op_;
};

template <typename T, typename Op, typename InvOp>
inline Reducer<T, Op, InvOp>& Reducer<T, Op, InvOp>::operator<<(const T& value) {
    agent_type* agent = combiner_.get_or_create_tls_agent();
    if (agent == nullptr) {
        // LOG
        return *this;
    }
    agent->element.modify(combiner_.op(), value);
    return *this;
}

}  // namespace contention_prof
