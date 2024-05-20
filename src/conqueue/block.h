#pragma once

#include "default_traits.h"
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
    Block* next_;   // 指向下一个Block
    std::atomic<size_t> elements_completely_dequeued_;  // 元素完全出队的个数. [隐式生产者会用到]

    // 块的位图,标志那个位置为空. [显示生产者会用到]
    // 和elements的对应关系是: empty_flags从后向前 --> elements从前向后
    std::atomic<bool> empty_flags_[kBlockSize <= kExplicitBlockEmptyCounterThreshold ? kBlockSize : 1];
    std::atomic<std::uint32_t> free_list_refs_;     // 
    std::atomic<Block<T>*> free_list_next_;         // 指向下一个空闲的块链表
    bool dynamically_allocated_;                    // 是否是动态分配
private:
    // 存储队列中的元素, 元素类型为char,大小是 sizeof(T) * KBlockSize
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



/**
 * @brief 判断块是否为空.隐式生产者使用原子变量判断,显式生产者使用位图
 * 
 * @return true 
 * @return false 
 */
template <typename T>
template <InnerQueueContext context>
bool Block<T>::IsEmpty() const
{
// 显式生产者
    if (context == explicit_context 
        && kBlockSize <= kExplicitBlockEmptyCounterThreshold)
    {
        for (size_t i = 0; i < kBlockSize; i++)
        {
            // 位图中相应位是否为false,为false表示为空
            // 获取empty_flags中对应位置,empty_flags是从后向前对应着elements的
            if (!empty_flags_[i].load(std::memory_order_relaxed))
                return false;
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        return true;
    }
// 隐式生产者
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


/**
 * @brief 设置块为空.对于显式生产者,将对应位设置为true.隐式生产者将原子变量减1
 * 
 * @param i 
 * @return true 
 * @return false 
 */
template <typename T>
template <InnerQueueContext context>
bool Block<T>::SetEmpty(index_t i)
{
    if (context == explicit_context 
        && kBlockSize <= kExplicitBlockEmptyCounterThreshold)
    {
        // 设置位图的相应位为true,表示该位置对应的数组位置为空
        // 获取empty_flags中对应位置,empty_flags是从后向前对应着elements的
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



/**
 * @brief 将一些块设置为空.显式生产者
 * 
 * @param i 
 * @param count 
 * @return true 
 * @return false 
 */
template <typename T>
template <InnerQueueContext context>
bool Block<T>::SetManyEmpty(index_t i, size_t count)
{
// 显式生产者
    if (context == explicit_context 
        && kBlockSize <= kExplicitBlockEmptyCounterThreshold) 
    {
        std::atomic_thread_fence(std::memory_order_release);
        // 获取empty_flags中对应位置,empty_flags是从后向前对应着elements的
        i = kBlockSize - 1 - static_cast<size_t>(
            i & static_cast<index_t>(kBlockSize - 1)) - count + 1;
        for (size_t j = 0; j != count; ++j) 
        {
        	assert(!empty_flags_[i + j].load(std::memory_order_relaxed));
        	empty_flags_[i + j].store(true, std::memory_order_relaxed);
        }
		return false;
	}
// 隐式生产者
    else 
    {
        // 将元素出队个数减去count
    	auto prevVal = elements_completely_dequeued_.fetch_add(
                count, std::memory_order_release);
    	assert(prevVal + count <= kBlockSize);
    	return prevVal + count == kBlockSize;
    }
}



/**
 * @brief 设置块都为空
 * 
 */
template <typename T>
template <InnerQueueContext context>
void Block<T>::SetAllEmpty()
{
// 显式生产者
    if (context == explicit_context 
    	&& kBlockSize <= kExplicitBlockEmptyCounterThreshold) 
    {
    	for (size_t i = 0; i != kBlockSize; ++i) 
    		empty_flags_[i].store(true, std::memory_order_relaxed);
    }
// 隐式生产者
    else 
    {
    	// Reset counter
    	elements_completely_dequeued_.store(kBlockSize, std::memory_order_relaxed);
    }
}


/**
 * @brief 将块重置为默认值.显式生产者将位图都设置为false.隐式生产者将原子变量出队个数设置0
 * 
 */
template <typename T>
template <InnerQueueContext context>
void Block<T>::ResetEmpty()
{
    if (context == explicit_context 
    	&& kBlockSize <= kExplicitBlockEmptyCounterThreshold) 
    {
    	// 重置位图
    	for (size_t i = 0; i != kBlockSize; ++i) 
    		empty_flags_[i].store(false, std::memory_order_relaxed);
    }
    else 
    {
    	// 重置计数器
    	elements_completely_dequeued_.store(0, std::memory_order_relaxed);
    }
}

/**
 * @brief 获取底层元素数组中相应位置的地址
 * 
 * @param index 
 * @return T* 
 */
template <typename T>
T* Block<T>::operator[](index_t index) noexcept
{
    return static_cast<T*>(static_cast<void*>(elements)) 
        + static_cast<size_t>(index & static_cast<index_t>(kBlockSize - 1));
}


/**
 * @brief 获取底层元素数组中相应位置的地址
 * 
 * @param index 
 * @return T* 
 */
template <typename T>
T const* Block<T>::operator[](index_t index) const noexcept
{
    return static_cast<T const*>(static_cast<void const*>(elements)) 
        + static_cast<size_t>(index & static_cast<index_t>(kBlockSize - 1));
}