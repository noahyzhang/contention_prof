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

#include <vector>
#include <atomic>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include "thread_local.h"

namespace contention_prof {

template <typename T, size_t ITEM_COUNT>
struct ObjectPoolFreeChunk {
    size_t free_count;
    T* ptrs[ITEM_COUNT];
};

static const size_t OP_MAX_BLOCK_GROUP_COUNT = 65536;
static const size_t OP_GROUP_BLOCK_BIT_COUNT = 16;
static const size_t OP_GROUP_BLOCK_COUNT = (1UL << OP_GROUP_BLOCK_BIT_COUNT);
static const size_t OP_INITIAL_FREE_LIST_SIZE = 1024;

template <typename T>
class ObjectPoolBlockItemNum {
    static const size_t N1 = ObjectPoolBlockMaxSize<T>::value / sizeof(T);
    static const size_t N2 = (N1 < 1 ? 1 : N1);
    static const size_t value = (N2 > ObjectPoolBlockMaxItem<T>::value ? ObjectPoolBlockMaxItem<T>::value : N2);
};

template <typename T>
class ObjectPool {
public:
    static const size_t BLOCK_ITEM_COUNT = ObjectPoolBlockItemNum<T>::value;
    static const size_t FREE_CHUNK_ITEM_COUNT = BLOCK_ITEM_COUNT;

    using FreeChunk = ObjectPoolFreeChunk<T, FREE_CHUNK_ITEM_COUNT>;
    using DynamicFreeChunk = ObjectPoolFreeChunk<T, 0>;

    struct Block {
        char items[sizeof(T) * BLOCK_ITEM_COUNT];
        size_t item_count;

        Block() : item_count(0) {}
    }

    struct BlockGroup {
        std::atomic<size_t> block_count;
        std::atomic<Block*> blocks[OP_GROUP_BLOCK_COUNT];

        BlockGroup() : block_count(0) {
            memset(blocks, 0, sizeof(std::atomic<Block*>) * OP_GROUP_BLOCK_COUNT);
        }
    };

    class LocalPool {
    public:
        explicit LocalPool(ObjectPool* pool)
            : pool_(pool),
            cur_block_(nullptr),
            cur_block_index_(0) {
            cur_free_.free_count = 0;
        }

        ~LocalPool() {
            if (cur_free.free_count) {
                pool_->push_free_chunk(cur_free_);
            }
            pool_->clear_from_destructor_of_local_pool();
        }

        static void delete_local_pool(void* arg) {
            delete reinterpret_cast<LocalPool*>(arg);
        }

        inline T* get() {

        }

        template <typename A1>
        inline T* get(const A1& a1) {

        }

        template <typename A1, typename A2>
        inline T* get(const A1& a1, const A2& a2) {

        }

        inline int return_object(T* ptr) {
            if (cur_free_.free_count < ObjectPool::free_chunk_item_count()) {
                cur_free_.ptrs[cur_free_.free_count++] = ptr;
                return 0;
            }
            if (pool_->push_free_chunk(cur_free_)) {
                cur_free_.free_count = 1;
                cur_free_.ptrs[0] = ptr;
                return 0;
            }
            return -1;
        }

    private:
        ObjectPool* pool_;
        Block* cur_block_;
        size_t cur_block_index_;
        FreeChunk cur_free_;
    };

    inline T* get_object() {
        LocalPool* lp = get_or_new_local_pool();
        if (__glibc_likely(lp != nullptr)) {
            return lp->get();
        }
        return nullptr;
    }

    template <typename A1>
    inline T* get_object(const A1& arg1) {
        LocalPool* lp = get_or_new_local_pool();
        if (__glibc_likely(lp != nullptr)) {
            return lp->get(arg1);
        }
        return nullptr;
    }

    template <typename A1, typename A2>
    inline T* get_object(const A1& arg1, const A2& arg2) {
        LocalPool* lp = get_or_new_local_pool();
        if (__glibc_likely(lp != nullptr)) {
            return lp->get(arg1, arg2);
        }
        return nullptr;
    }

    inline int return_object(T* ptr) {
        LocalPool* lp = get_or_new_local_pool();
        if (__glibc_likely(lp != nullptr)) {
            return lp->return_object(ptr);
        }
        return -1;
    }

    void clear_objects() {
        LocalPool* lp = local_pool_;
        if (lp) {
            local_pool_ = nullptr;
            thread_atexit_cancel(LocalPool::delete_local_pool, lp);
            delete lp;
        }
    }

    inline static size_t free_chunk_item_count() {
        const size_t n = ObjectPoolFreeChunkMaxItem<T>::value();
        return (n < FREE_CHUNK_ITEM_COUNT ? n : FREE_CHUNK_ITEM_COUNT);
    }

    ObjectPoolInfo describe_objects() const {

    }

    static inline ObjectPool* get_instance() {
        static ObjectPool instance;
        return &instance;
    }

private:
    ObjectPool() {
        free_chunks_.reserve(OP_INITIAL_FREE_LIST_SIZE);
        pthread_mutex_init(&free_chunks_mutex_, nullptr);
    }

    ~ObjectPool() {
        pthread_mutex_destroy(&free_chunks_mutex_);
    }

    static Block* add_block(size_t* index) {
        Block* const new_block = new Block();
        if (new_block == nullptr) {
            return nullptr;
        }
        size_t group_count;
        do {
            group_count = group_count_.load(std::memory_order_acquire);
            if (group_count >= 1) {
                BlockGroup* const g = block_groups_[group_count - 1].load(std::memory_order_consume);
                const size_t block_index = g->block_count.fetch_add(1, std::memory_order_relaxed);
                if (block_index < OP_GROUP_BLOCK_COUNT) {
                    g->blocks[block_index].store(new_block, std::memory_order_release);
                    *index = (group_count - 1) * OP_GROUP_BLOCK_COUNT + block_index;
                    return new_block;
                }
                g->block_count.fetch_sub(1, std::memory_order_relaxed);
            }
        } while (add_block_group(group_count));
        delete new_block;
        return nullptr;
    }

    static bool add_block_group(size_t old_group_count) {
        BlockGroup* bg = nullptr;
        pthread_mutex_lock(&block_group_mutex_);
        const size_t group_count = group_count_.load(std::memory_order_acquire);
        if (group_count != old_group_count) {
            pthread_mutex_unlock(&block_group_mutex_);
            return true;
        }
        if (group_count < OP_MAX_BLOCK_GROUP_COUNT) {
            bg = new BlockGroup();
            if (bg != nullptr) {
                block_groups_[group_count].store(bg, std::memory_order_release);
                group_count_.store(group_count+1, std::memory_order_release);
            }
        }
        pthread_mutex_unlock(&block_group_mutex_);
        return bg != nullptr;
    }

    inline LocalPool* get_or_new_local_pool() {
        LocalPool* lp = local_pool_;
        if (__glibc_likely(lp != nullptr)) {
            return lp;
        }
        lp = new LocalPool(this);
        if (lp == nullptr) {
            return nullptr;
        }
        pthread_mutex_lock(&change_thread_mutex_);
        local_pool_ = lp;
        thread_atexit(LocalPool::delete_local_pool, lp);
        local_count_.fetch_add(1, std::memory_order_relaxed);
        return lp;
    }

    void clear_from_destructor_of_local_pool() {
        local_pool_ = nullptr;
        if (local_pool_.fetch_sub(1, std::memory_order_relaxed) != 1) {
            return;
        }
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
        c.free_count = p->free_count;
        memcpy(c.ptrs, p->ptrs, sizeof(*p->ptrs) * p->free_count);
        free(p);
        return true;
    }

    bool push_free_chunk(const FreeChunk& c) {
        DynamicFreeChunk* p = reinterpret_cast<DynamicFreeChunk*>(malloc(
            offsetof(DynamicFreeChunk, ptrs) + sizeof(*c.ptrs) * c.free_count));
        if (!p) {
            return false;
        }
        p->free_count = c.free_count;
        memcpy(p->ptrs, c.ptrs, sizeof(*c.ptrs) * c.free_count);
        pthread_mutex_lock(&free_chunks_mutex_);
        free_chunks_.push_back(p);
        pthread_mutex_unlock(&free_chunks_mutex_);
        return true;
    }

private:
    static __thread LocalPool* local_pool_;
    static std::atomic<int64_t> local_count_;
    static std::atomic<size_t> group_count_;
    static pthread_mutex_t block_group_mutex_;
    static pthread_mutex_t change_thread_mutex_;
    static std::atomic<BlockGroup*> block_groups_[OP_MAX_BLOCK_NGROUP];

    std::vector<DynamicFreeChunk*> free_chunks_;
    pthread_mutex_t free_chunks_mutex_;
};



}  // namespace contention_prof
