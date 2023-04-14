/**
 * @file linked_list.h
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

/**
 * @brief 双向链表的节点实现
 * 
 * @tparam T 
 */
template <typename T>
class LinkNode {
public:
    LinkNode() : previous_(this), next_(this) {}

    LinkNode(LinkNode<T>* previous, LinkNode<T>* next)
        : previous_(previous), next_(next) {}

    void insert_before(LinkNode<T>* e) {
        this->next_ = e;
        this->previous_ = e->previous_;
        e->previous_->next_ = this;
        e->previous_ = this;
    }

    void insert_before_as_list(LinkNode<T>* e) {
        LinkNode<T>* prev = this->previous_;
        prev->next_ = e;
        this->previous_ = e->previous_;
        e->previous_->next_ = this;
        e->previous_ = prev;
    }

    void insert_after(LinkNode<T>* e) {
        this->next_ = e->next_;
        this->previous_ = e;
        e->next_->previous_ = this;
        e->next_ = this;
    }

    void insert_after_as_list(LinkNode<T>* e) {
        LinkNode<T>* prev = this->previous_;
        prev->next_ = e->next_;
        this->previous_ = e;
        e->next_->previous_ = prev;
        e->next_ = this;
    }

    void remove_from_list() {
        this->previous_->next_ = this->next_;
        this->next_->previous_ = this->previous_;
        this->next_ = this;
        this->previous_ = this;
    }

    LinkNode<T>* previous() const {
        return previous_;
    }

    LinkNode<T>* next() const {
        return next_;
    }

    const T* value() const {
        return static_cast<const T*>(this);
    }

    T* value() {
        return static_cast<T*>(this);
    }

private:
    LinkNode<T>* previous_;
    LinkNode<T>* next_;
};

/**
 * @brief 双向链表的实现
 * 
 * @tparam T 
 */
template <typename T>
class LinkedList {
public:
    LinkedList() {}

    void Append(LinkNode<T>* e) {
        e->insert_before(&root_);
    }

    LinkNode<T>* head() const {
        return root_.next();
    }

    LinkNode<T>* tail() const {
        return root_.previous();
    }

    const LinkNode<T>* end() const {
        return &root_;
    }

    bool empty() const {
        return head() == end();
    }

private:
    LinkNode<T> root_;
};

}  // namespace contention_prof
