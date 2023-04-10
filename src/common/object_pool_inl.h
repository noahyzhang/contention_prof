/**
 * @file object_pool_inl.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <pthread.h>
#include <vector>
#include <atomic>

namespace noahyzhang {
namespace contention_prof {

template <typename T, size_t NITEM>
struct ObjectPoolFreeChunk {
    size_t nfree;
    T* ptrs[NITEM];
};

template <typename T>
class ObjectPoolBlockItemNum {
    static const size_t N1 = ObjectPoolBlockMaxSize<T>::value / sizeof(T);
    static const size_t N2 = (N1 < 1 ? 1 : N1);
    static const size_t value = (N2 > ObjectPoolBlockMaxItem<T>::value ? ObjectPoolBlockMaxItem<T>::value : N2);
};

template <typename T>
class ObjectPool {
public:
    struct Block {
        char items[sizeof(T) * BLOCK_NITEM];
        size_t nitem;

        Block() : nitem(0) {}
    }

    struct BlockGroup {
        std::atomic<size_t> nblock_;
        std::atomic<Block*> blocks[OP_GROUP_NBLOCK];

        BlockGroup() : nblock_(0) {
            memset(blocks, 0, sizeof(std::atomic<Block*>) * OP_GROUP_NBLOCK);
        }
    };

    class LocalPool {
    public:
        explicit LocalPool(ObjectPool* pool)
            : pool_(pool),
            cur_block_(nullptr),
            cur_block_index_(0) {
            cur_free_.nfree = 0;
        }
        ~LocalPool() {
            if (cur_free.nfree) {
                pool_->push
            }
        }

    private:
        ObjectPool* pool_;
        Block* cur_block_;
        size_t cur_block_index_;
        FreeChunk cur_free_;
    };

private:
    ObjectPool() {

    }

private:
    bool pop_free_chunk(FreeChunk& c) {
        if (free_chunks_.empty()) {
            return false;
        }
        pthread_mutex_lock(&free_chunks_mutex_);
        if (free_chunks_.empty()) {
            pthread_mutex_unlock(&free_chunks_mutex_);
            return false;
        }
        DynamicFreeChunk* p = free_chunks_.back();
        free_chunks_.pop_back();
        pthread_mutex_unlock(&free_chunks_mutex_);
        c.nfree = p->nfree;
        memcpy(c.ptrs, p->ptrs, sizeof(*p->ptrs) * p->nfree);
        free(p);
        return true;
    }

    bool push_free_chunk(const FreeChunk& c) {
        DynamicFreeChunk* p = (DynamicFreeChunk*)malloc(
            offsetof(DynamicFreeChunk, ptrs) + sizeof(*c.ptrs) * c.nfree);
        if (!p) {
            return false;
        }
        p->nfree = c.nfree;
        memcpy(p->ptrs, c.ptrs, sizeof(*c.ptrs) * c.nfree);
        pthread_mutex_lock(&free_chunks_mutex);
        free_chunks.push_back(p);
        pthread_mutex_unlock(&free_chunks_mutex);
        return true;
    }

private:
    static std::atomic<ObjectPool*> singleton_;
    static pthread_mutex_t singleton_mutex_;
    static __thread LocalPool* local_pool_;
    static std::atomic<long> nlocal_;
    static std::atomic<size_t> ngroup_;
    static pthread_mutex_t block_group_mutex_;
    static pthread_mutex_t change_thread_mutex_;
    static std::atomic<BlockGroup*> block_groups[OP_MAX_BLOCK_NGROUP];

    std::vector<DynamicFreeChunk*> free_chunks_;
    pthread_mutex_t free_chunks_mutex_;
};

}  // namespace contention_prof
}  // namespace noahyzhang
