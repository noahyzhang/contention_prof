/**
 * @file combiner.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <mutex>
#include "common/linked_list.h"

namespace contention_prof {

template <typename T, typename Enabler = void>
class ElementContainer {
public:
    void load(T* out) {
        std::lock_guard<std::mutex> guard(mtx_);
        *out = value_;
    }

    void store(const T& new_value) {
        std::lock_guard<std::mutex> guard(mtx_);
        value_ = new_value;
    }

    void exchange(T* prev, const T& new_value) {
        std::lock_guard<std::mutex> guard(mtx_);
        *prev = value_;
        value_ = new_value;
    }

    template <typename Op, typename T1>
    void modify(const Op& op, const T1& value2) {
        std::lock_guard<std::mutex> guard(mtx_);
        call_op_retuning_void(op, value_, value2);
    }

    template <typename Op, typename GlobalValue>
    void merge_global(const op& op, GlobalValue& global_value) {
        std::lock_guard<std::mutex> guard(mtx_);
        op(global_value, value_);
    }

private:
    T value_;
    std::mutex mtx_;
};

template <typename ResultTp, typename ElementTp, typename BinaryOp>
class AgentCombiner {
public:
    using self_type = AgentCombiner<ResultTp, ElementTp, BinaryOp>;
    friend class GlobalValue<self_type>;

public:
    struct Agent : public LinkNode<Agent> {
    public:
        Agent() : combiner(nullptr) {}

        ~Agent() {
            if (combiner) {
                combiner->commit_and_erase(this);
                combiner = nullptr;
            }
        }

        void reset(const ElementTp& val, self_type* c) {
            combiner = c;
            element.store(val);
        }

        template <typename Op>
        void merge_global(const Op& op) {
            GlobalValue<self_type> g(this, combiner);
            element.merge_global(op, g);
        }

    public:
        self_type* combiner;
        ElementContainer<ElementTp> element;
    };

    using AgentGroup = AgentGroup<Agent>;

    explicit AgentCombiner(
        const ResultTp result_identify = ResultTp(),
        const ElementTp element_identify = ElementTp(),
        const BinaryOp& op = BinaryOp())
        : id_(AgentGroup::create_new_agent())
        , op_(op)
        , global_result_(result_identify)
        , result_identify_(result_identify)
        , element_identify_(element_identify) {}

    ~AgentCombiner() {
        if (id_ >= 0) {
            clear_all_agents();
            AgentGroup::destory_agent(id_);
            id_ = -1;
        }
    }

    ResultTp combine_agents() const {
        ElementTp tls_value;
        std::lock_guard<std::mutex> guard(mtx_);
        ResultTp ret = global_result_;
        for (LinkNode<Agent>* node = agents_.head(); node != agents_.end(); node = node->next()) {
            node->value()->element.load(&tls_value);
            op_(ret, tls_value);
        }
        return res;
    }

    ResultTp reset_all_agents() {
        ElementTp prev;
        std::lock_guard<std::mutex> guard(mtx_);
        ResultTp tmp = global_result_;
        global_result_ = result_identify_;
        for (LinkNode<Agent>* node = agents_.head(); node != agents_.end(); node = node->next()) {
            node->value()->element.exchange(&prev, element_identify_);
            call_op_returning_void(op_, tmp, prev);
        }
        return tmp;
    }

    void commit_and_erase(Agent* agent) {
        if (agent == nullptr) {
            return;
        }
        ElementTp local;
        std::lock_guard<std::mutex> guard(mtx_);
        agent->element.load(&local);
        call_op_returning_void(op_, global_result_, local);
        agent->remove_from_list();
    }

    void commit_and_clear(Agent* agent) {
        if (agent == nullptr) {
            return;
        }
        ElementTp prev;
        std::lock_guard<std::mutex> guard;
        agent->element.exchange(&prev, element_identify_);
        call_op_returning_void(op_, global_result_, prev);
    }

    inline Agent* get_or_create_tls_agent() {
        Agent* agent = AgentGroup::get_tls_agent(id_);
        if (agent == nullptr) {
            agent = AgentGroup::get_or_create_tls_agent(id_);
            if (agent == nullptr) {
                return nullptr;
            }
        }
        if (agent->combiner) {
            return agent;
        }
        agent->reset(element_identify_, this);
        {
            std::lock_guard<std::mutex> guard(mtx_);
            agents_.Append(agent);
        }
        return agent;
    }

    void clear_all_agents() {
        std::lock_guard<std::mutex> guard(mtx_);
        for (LinkNode<Agent>* node = agents_.head(); node != agents_.end();) {
            node->value()->reset(ElementTp(), nullptr);
            LinkNode<Agent>* const saved_next = node->next();
            node->remove_from_list();
            node = saved_next;
        }
    }

    const BinaryOp& op() const { return op_; }

    bool valid() const { return id_ >= 0; }

private:
    int id_;
    BinaryOp op_;
    mutable std::mutex mtx_;
    ResultTp global_result_;
    ResultTp result_identify_;
    ElementTp element_identify_;
    LinkedList<Agent> agents_;
};

}  // namespace contention_prof
