#pragma once

#include "details.h"

#include <cstdint>

template<typename T> class BlockingConcurrentQueue;

template <typename T> class ConcurrentQueue;

struct ConsumerToken
{
    template<typename T>
	explicit ConsumerToken(ConcurrentQueue<T>& queue);
	
	template<typename T>
	explicit ConsumerToken(BlockingConcurrentQueue<T>& queue);

    ConsumerToken(ConsumerToken&& other) noexcept;
    ConsumerToken& operator=(ConsumerToken&& other) noexcept;

    void Swap(ConsumerToken& other) noexcept;

    ConsumerToken(ConsumerToken const&) = delete;
    ConsumerToken& operator=(ConsumerToken const&) = delete;
private:
    template<typename T> friend class ConcurrentQueue;
	friend class ConcurrentQueueTests;
private:
    std::uint32_t initial_offset_;
	std::uint32_t last_known_global_offset_;
	std::uint32_t items_consumed_from_current_;
	ConcurrentQueueProducerTypelessBase* current_producer_;
	ConcurrentQueueProducerTypelessBase* desired_producer_;
};





/////////////////////////////////////////////////// 实现
template<typename T>
ConsumerToken::ConsumerToken(ConcurrentQueue<T>& queue)
    : items_consumed_from_current_(0),
    current_producer_(nullptr),
    desired_producer_(nullptr)
{
    initial_offset_ = queue.next_explicit_consumer_id_.fetch_add(1, std::memory_order_release);
    last_known_global_offset_ = static_cast<std::uint32_t>(-1);
}

template<typename T>
ConsumerToken::ConsumerToken(BlockingConcurrentQueue<T>& queue)
    : items_consumed_from_current_(0), 
    current_producer_(nullptr), desired_producer_(nullptr)
{
    initial_offset_ = reinterpret_cast<ConcurrentQueue<T>*>(&queue)
        ->next_explicit_consumer_id_.fetch_add(1, std::memory_order_release);
    last_known_global_offset_ = static_cast<std::uint32_t>(-1);
}