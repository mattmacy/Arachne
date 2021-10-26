/* Copyright (c) 2021 Matthew Macy
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <memory>
#include <assert.h>

#pragma once
struct intrusive_list_node
{
    using node_ptr = intrusive_list_node *;
    node_ptr next_;
    node_ptr prev_;
    intrusive_list_node() : next_(nullptr), prev_(nullptr) {}
    intrusive_list_node(intrusive_list_node&) = delete;
    intrusive_list_node(intrusive_list_node&&) = delete;
};

struct default_tag;

template<typename Tag=default_tag>
class intrusive_list_base_hook : public intrusive_list_node
{

    using node_ptr = intrusive_list_node *;
    using tag = Tag;
    template<class, class> friend class intrusive_list;
    template<class, class> friend class _intrusive_list_iterator;
public:
    bool is_linked() {
        return this->next_ != nullptr;
    }

    void unlink() {
        node_ptr next{this->next_};
        node_ptr prev{this->prev_};
        prev->next_ = next;
        next->prev_ = prev;
        this->prev_ = this->next_ = nullptr;
    }

    void link_before(node_ptr next_node) {
        node_ptr prev{next_node->prev_};
        assert(this->next_ == nullptr);
        this->prev_ = prev;
        this->next_ = next_node;
        next_node->prev_ = static_cast<node_ptr>(this);
        prev->next_ = static_cast<node_ptr>(this);
    }

    void link_after(node_ptr prev_node) {
        node_ptr next{prev_node->next_};
        assert(this->next_ == nullptr);
        this->prev_ = prev_node;
        this->next_ = next;
        prev_node->next_ = static_cast<node_ptr>(this);
        next->prev_ = static_cast<node_ptr>(this);
    }
};

template <typename T, typename tag = default_tag>
class __intrusive_list_iterator {

    using node_ptr = intrusive_list_node *;

    node_ptr ptr;
    explicit __intrusive_list_iterator(node_ptr p) noexcept : ptr(p) {}

    template<class, class> friend class intrusive_list;
    template<class> friend class intrusive_list_base_hook;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using reference = value_type&;
    using pointer = typename  std::pointer_traits<node_ptr>::template rebind<value_type>;
    
    __intrusive_list_iterator() noexcept : ptr(nullptr) {}


    reference operator*() const
    {
        return static_cast<reference>(static_cast<intrusive_list_base_hook<tag> &>(*ptr));
    }

    
    pointer operator->() const
    {
        return static_cast<pointer>(static_cast<intrusive_list_base_hook<tag> *>(ptr));
    }

    __intrusive_list_iterator& operator++()
    {
        ptr = ptr->next_;
        return *this;
    }

    __intrusive_list_iterator operator++(int) {__intrusive_list_iterator __t(*this); ++(*this); return __t;}

    __intrusive_list_iterator& operator--()
    {
        ptr = ptr->prev_;
        return *this;
    }
    __intrusive_list_iterator operator--(int) {__intrusive_list_iterator __t(*this); --(*this); return __t;}

    friend
    bool operator==(const __intrusive_list_iterator& __x, const __intrusive_list_iterator& __y)
    {
        return __x.ptr == __y.ptr;
    }
    friend
    bool operator!=(const __intrusive_list_iterator& __x, const __intrusive_list_iterator& __y)
        {return !(__x == __y);}
};

template<typename T, typename tag = default_tag>
class intrusive_list {
    using node_ptr = intrusive_list_node *;
    using value_type = T;
    using reference = value_type&;
    using const_reference = const value_type &;
    using iterator = __intrusive_list_iterator<T, tag>;

    intrusive_list_node node;

public:
    intrusive_list(intrusive_list &) = delete;

    intrusive_list(intrusive_list &&) = delete;

    intrusive_list() {
        node.next_ = node.prev_ = &node;
    }

    bool empty() {
        return node.next_ == &node;
    }

    reference front() {
        return static_cast<reference>(*node.next_);
    }

    reference back() {
        return static_cast<reference>(*node.prev_);
    }

    void push_front(reference value) {
        auto hook_ptr = static_cast<intrusive_list_base_hook<tag> *>(std::addressof(value));
        hook_ptr->link_after(&this->node);
    }

    void push_back(reference value) {
        auto hook_ptr = static_cast<intrusive_list_base_hook<tag> *>(std::addressof(value));
        hook_ptr->link_before(&this->node);
    }

    void pop_front() {
        assert(node.next_ != &node);
        auto hook_ptr = static_cast<intrusive_list_base_hook<tag> *>(node.next_);
        hook_ptr->unlink();
    } 

    void pop_back() {
        assert(node.prev_ != &node);
        auto hook_ptr = static_cast<intrusive_list_base_hook<tag> *>(node.prev_);
        hook_ptr->unlink();
    }

    iterator begin() {
        return iterator(node.next_);
    }

    iterator end() {
        return iterator(&node);
    }
    
};
