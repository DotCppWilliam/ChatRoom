#pragma once

#include "details.h"
#include <cassert>
#include <cstdint>
#include <atomic>

template <typename Node>
struct FreeListNode
{
    FreeListNode() : free_list_refs_(0), free_list_next_(nullptr) {}

    std::atomic<std::uint32_t> free_list_refs_;
    std::atomic<Node*> free_list_next_;
};


template <typename Node>
struct FreeList 
{
    FreeList();
    FreeList(FreeList&& other);
    
    FreeList(FreeList const&) = delete;
    FreeList& operator=(FreeList const&) = delete;

    void Add(Node* node);
    Node* TryGet();
    Node* HeadUnsafe() const;
    void Swap(FreeList& other) { SwapRelaxed(free_list_head_, other.free_list_head_); }

private:
    void AddKnowingRefcountIsZero(Node* node);
private:
    std::atomic<Node*> free_list_head_;

    static const std::uint32_t kRefsMask = 0x7FFFFFFF;
    static const std::uint32_t kShouldBeOnFreeList = 0x80000000;
};






////////////////////////////////////////////// 实现
template <typename Node>
FreeList<Node>::FreeList()
    : free_list_head_(nullptr) {}

template <typename Node>
FreeList<Node>::FreeList(FreeList&& other)
    : free_list_head_(other.free_list_head_.load(std::memory_order_relaxed))
{
    other.free_list_head_.store(nullptr, std::memory_order_relaxed);
}


template <typename Node>
void FreeList<Node>::Add(Node* node)
{
    if (0 == node->free_list_refs_.fetch_add(
            kShouldBeOnFreeList, std::memory_order_acq_rel))
    {
        AddKnowingRefcountIsZero(node);
    }
}

template <typename Node>
Node* FreeList<Node>::TryGet()
{
    auto head = free_list_head_.load(std::memory_order_acquire);
    while (head != nullptr)
    {
        auto prev_head = head;
        auto refs = head->free_list_refs_.load(std::memory_order_acquire);
        if ((refs & kRefsMask) == 0
            || !head->free_list_refs_.compare_exchange_strong(refs, 
                refs + 1, std::memory_order_acquire, std::memory_order_relaxed))
        {
            head = free_list_head_.load(std::memory_order_acquire);
            continue;
        }

        auto next = head->free_list_next_.load(std::memory_order_relaxed);
        if (free_list_head_.compare_exchange_strong(head, next, std::memory_order_acquire,
            std::memory_order_relaxed))
        {
            assert((head->free_list_refs_.load(std::memory_order_relaxed)
                &  kShouldBeOnFreeList) == 0);
            head->free_list_refs_.fetch_sub(2, std::memory_order_release);
            return head;
        }

        refs = prev_head->free_list_refs_.fetch_sub(1, std::memory_order_acq_rel);
        if (refs == kShouldBeOnFreeList + 1)
            AddKnowingRefcountIsZero(prev_head);
    }
    return nullptr;
}

template <typename Node>
Node* FreeList<Node>::HeadUnsafe() const
{ 
    return free_list_head_.load(std::memory_order_relaxed);
}

template <typename Node>
void FreeList<Node>::AddKnowingRefcountIsZero(Node* node)
{ 
    auto head = free_list_head_.load(std::memory_order_relaxed);
    while (true)
    {
        node->free_list_next_.store(head, std::memory_order_relaxed);
        node->free_list_refs_.store(1, std::memory_order_relaxed);
        if (!free_list_head_.compare_exchange_strong(head, node, std::memory_order_release,
            std::memory_order_relaxed))
        {
            if (node->free_list_refs_.fetch_add(kShouldBeOnFreeList - 1, 
                std::memory_order_release) == 1)
            {
                continue;
            }
        }
        return;
    }
}