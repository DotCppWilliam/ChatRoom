#pragma once

#include "concurrent_queue_default_traits.h"
#include "details.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>


enum InnerQueueContext { implicit_context = 0, explicit_context = 1 };

template <typename T>
struct Block 
{
    Block();

    template <InnerQueueContext context>
    bool IsEmpty() const;

    template <InnerQueueContext context>
    bool SetEmpty(index_t i);
    
    template <InnerQueueContext context>
    bool SetManyEmpty(index_t i, size_t count);

    template <InnerQueueContext context>
    void SetAllEmpty();

    template <InnerQueueContext context>
    void ResetEmpty();

    T* operator[](index_t index) noexcept;
    T const* operator[](index_t index) const noexcept;
public:
    Block* next_;
    std::atomic<size_t> elements_completely_dequeued_;  // 元素完全出队的个数

    // 块的位图,标志那个位置为空
    std::atomic<bool> empty_flags_[kBlockSize <= kExplicitBlockEmptyCounterThreshold
        ? kBlockSize : 1];
    std::atomic<std::uint32_t> free_list_refs_;
    std::atomic<Block<T>*> free_list_next_;
    bool dynamically_allocated_;

private:
    // 定义字符数组变量,大小是sizeof(T) *   kBlockSize
    alignas(alignof(T)) typename Identify<char[sizeof(T) * kBlockSize]>::type elements;
};




///////////////////////////////////// 实现
template <typename T>
Block<T>::Block()
    : next_(nullptr),
    elements_completely_dequeued_(0),
    free_list_refs_(0),
    free_list_next_(nullptr),
    dynamically_allocated_(true) { }

template <typename T>
template <InnerQueueContext context>
bool Block<T>::IsEmpty() const
{
    if (context == explicit_context 
        && kBlockSize <= kExplicitBlockEmptyCounterThreshold)
    {
        for (size_t i = 0; i < kBlockSize; i++)
        {
            if (!empty_flags_[i].load(std::memory_order_relaxed))
                return false;
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        return true;
    }
    else 
    {
        if (elements_completely_dequeued_.load(std::memory_order_relaxed)
            == kBlockSize)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            return true;
        }
        assert(elements_completely_dequeued_.load(std::memory_order_relaxed) <= kBlockSize);
        return false;
    }
}


template <typename T>
template <InnerQueueContext context>
bool Block<T>::SetEmpty(index_t i)
{
    if (context == explicit_context 
        && kBlockSize <= kExplicitBlockEmptyCounterThreshold)
    {
        // 设置位图的相应位为true,表示该位置对应的数组位置为空
        empty_flags_[kBlockSize - 1 - static_cast<size_t>(
            i & static_cast<index_t>(kBlockSize - 1))].store(true, std::memory_order_release);
        return false;
    }
    else 
    {
        // 出队个数加+1
        auto prev_val = elements_completely_dequeued_.fetch_add(1, std::memory_order_release);
        assert(prev_val < kBlockSize);  // 断言,判断出队个数是否大于块的大小了
        return prev_val == kBlockSize - 1;  // 如果为true表示块已经为空
    }
}

template <typename T>
template <InnerQueueContext context>
bool Block<T>::SetManyEmpty(index_t i, size_t count)
{
    if (context == explicit_context 
        && kBlockSize <= kExplicitBlockEmptyCounterThreshold) 
    {
        std::atomic_thread_fence(std::memory_order_release);
        i = kBlockSize - 1 - static_cast<size_t>(
            i & static_cast<index_t>(kBlockSize - 1)) - count + 1;
        for (size_t j = 0; j != count; ++j) 
        {
        	assert(!empty_flags_[i + j].load(std::memory_order_relaxed));
        	empty_flags_[i + j].store(true, std::memory_order_relaxed);
        }
		return false;
	}
    else 
    {
    	auto prevVal = elements_completely_dequeued_.fetch_add(
                count, std::memory_order_release);
    	assert(prevVal + count <= kBlockSize);
    	return prevVal + count == kBlockSize;
    }

}

template <typename T>
template <InnerQueueContext context>
void Block<T>::SetAllEmpty()
{
    if (context == explicit_context 
    	&& kBlockSize <= kExplicitBlockEmptyCounterThreshold) 
    {
    	for (size_t i = 0; i != kBlockSize; ++i) 
    		empty_flags_[i].store(true, std::memory_order_relaxed);
    }
    else 
    {
    	// Reset counter
    	elements_completely_dequeued_.store(kBlockSize, std::memory_order_relaxed);
    }
}


template <typename T>
template <InnerQueueContext context>
void Block<T>::ResetEmpty()
{
    if (context == explicit_context 
    	&& kBlockSize <= kExplicitBlockEmptyCounterThreshold) 
    {
    	// Reset flags
    	for (size_t i = 0; i != kBlockSize; ++i) 
    		empty_flags_[i].store(false, std::memory_order_relaxed);
    }
    else 
    {
    	// Reset counter
    	elements_completely_dequeued_.store(0, std::memory_order_relaxed);
    }
}


template <typename T>
T* Block<T>::operator[](index_t index) noexcept
{
    return static_cast<T*>(static_cast<void*>(elements)) 
        + static_cast<size_t>(index & static_cast<index_t>(kBlockSize - 1));
}

template <typename T>
T const* Block<T>::operator[](index_t index) const noexcept
{
    return static_cast<T const*>(static_cast<void const*>(elements)) 
        + static_cast<size_t>(index & static_cast<index_t>(kBlockSize - 1));
}