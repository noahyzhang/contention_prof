/**
 * @file agent_group.h
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
#include <deque>
#include <vector>
#include <algorithm>
#include "thread_local.h"

namespace contention_prof {

using AgentId = int;

template <typename Agent>
class AgentGroup {
public:
    using agent_type = Agent;
    static size_t RAW_BLOCK_SIZE = 4096;
    static size_t ELEMENT_PER_BLOCK = (RAW_BLOCK_SIZE + sizeof(Agent) - 1) / sizeof(Agent);

    struct ThreadBlock {
        inline Agent* at(size_t offset) {
            return agents_ + offset;
        }
    private:
        Agent agents_[ELEMENT_PER_BLOCK];
    };

    inline static int create_new_agent() {
        std::lock_guard<std::mutex> guard(mtx_);
        AgentId agent_id = 0;
        if (!get_free_ids().empty()) {
            agent_id = get_free_ids().back();
            get_free_ids().pop_back();
        } else {
            agent_id = agent_kinds_++;
        }
        return agent_id;
    }

    inline static int destroy_agent(AgentId id) {
        std::lock_guard<std::mutex> guard;
        if (id < 0 || id >= agent_kinds_) {
            errno = EINVAL;
            return -1;
        }
        get_free_ids().push_back(id);
        return 0;
    }

    inline static Agent* get_tls_agent(AgentId id) {
        if (__glibc_likely(id >= 0)) {
            if (tls_blocks_) {
                const size_t block_id = static_cast<size_t>(id) / ELEMENT_PER_BLOCK;
                if (block_id < tls_blocks_->size()) {
                    ThreadBlock* const tb = (*tls_blocks_)[block_id];
                    if (tb) {
                        return tb->at(id - block_id * ELEMENT_PER_BLOCK);
                    }
                }
            }
        }
        return nullptr;
    }

    inline static Agent* get_or_create_tls_agent(AgentId id) {
        if (__glibc_unlikely(id < 0)) {
            return nullptr;
        }
        if (tls_blocks_ == nullptr) {
            tls_blocks_ = new std::vector<ThreadBlock*>();
            if (__glibc_unlikely(tls_blocks_ == nullptr)) {
                return nullptr;
            }
            thread_atexit(destroy_tls_blocks);
        }
        const size_t block_id = static_cast<size_t>(id) / ELEMENT_PER_BLOCK;
        if (block_id >= tls_blocks_->size()) {
            tls_blocks_->resize(std::max(block_id + 1, 32UL));
        }
        ThreadBlock* tb = (*tls_blocks_)[block_id];
        if (tb == nullptr) {
            ThreadBlock* new_block = new ThreadBlock();
            if (__glibc_unlikely(new_block == nullptr)) {
                return nullptr;
            }
            tb = new_block;
            (*tls_blocks_)[block_id] = new_block;
        }
        return tb->at(id - block_id * ELEMENT_PER_BLOCK);
    }

private:
    static void destroy_tls_blocks() {
        if (!tls_blocks_) {
            return;
        }
        for (size_t i = 0; i < tls_blocks_->size(); ++i) {
            delete (*tls_blocks_)[i];
        }
        delete tls_blocks_;
        tls_blocks_ = nullptr;
    }

    inline static std::deque<AgentId>& get_free_ids() {
        if (free_ids_ == nullptr) {
            free_ids_ = new std::deque<AgentId>();
            if (free_ids_ == nullptr) {
                abort();
            }
        }
        return *free_ids_;
    }

private:
    static std::mutex mtx_;
    static AgentId agent_kinds_;
    static std::deque<AgentId>* free_ids_;
    static __thread std::vector<ThreadBlock*>* tls_blocks_;
};

template <typename Agent>
std::mutex AgentGroup<Agent>::mtx_;

template <typename Agent>
AgentId AgentGroup<Agent>::agent_kinds_ = 0;

template <typename Agent>
std::deque<AgentId>* AgentGroup<Agent>::free_ids_ = nullptr;

template <typename Agent>
__thread std::vector<typename AgentGroup<Agent>::ThreadBlock*>* AgentGroup<Agent>::tls_blocks_ = nullptr;

}  // namespace contention_prof
