#pragma once

#include "details.h"
#include "block.h"

#include <cstddef>

template <typename T> class ConcurrentQueue;

template <typename T>
struct ProducerBase : public ConcurrentQueueProducerTypelessBase
{
    ProducerBase(ConcurrentQueue<T>* parent, bool is_explicit);
    virtual ~ProducerBase() {}

    template <typename U>
    bool Dequeue(U& element);

    template <typename It>
    size_t DequeueBulk(It& item_first, size_t max);

    ProducerBase* NextProd() const;
    size_t SizeApprox() const;

    index_t GetTail() const;
public:
	bool is_explicit_;              // 是否是显式生产者
	ConcurrentQueue<T>* parent_;
protected:
	std::atomic<index_t> tail_index_;		
	std::atomic<index_t> head_index_;		
	
	std::atomic<index_t> dequeue_optimistic_count_; // 乐观出队计数器,用于在没有锁的情况下进行出队操作估计
	std::atomic<index_t> dequeue_overcommit_;       // 用于记录尝试出队但失败的次数,以帮助避免竞争和冲突
	
	Block<T>* tail_block_;
};


template <typename T>
class ExplicitProducer;

template <typename T>
class ImplicitProducer;


/////////////////////////////////////// 实现
template <typename T>
ProducerBase<T>::ProducerBase(ConcurrentQueue<T>* parent, bool is_explicit)
    : tail_index_(0),
    head_index_(0),
    dequeue_optimistic_count_(0),
    dequeue_overcommit_(0),
    tail_block_(nullptr),
    is_explicit_(is_explicit),
    parent_(parent) { }

template <typename T>
template <typename U>
bool ProducerBase<T>::Dequeue(U& element)
{
    if (is_explicit_)
        return static_cast<ExplicitProducer<T>*>(this)->Dequeue(element);
    else
        return static_cast<ImplicitProducer<T>*>(this)->Dequeue(element);;
    return false;
}

template <typename T>
template <typename It>
size_t ProducerBase<T>::DequeueBulk(It& item_first, size_t max)
{
    if (is_explicit_)
        return static_cast<ExplicitProducer<T>*>(this)->DequeueBulk(item_first, max);
    else
        return static_cast<ImplicitProducer<T>*>(this)->DequeueBulk(item_first, max);;
    return false;
}

template <typename T>
ProducerBase<T>* ProducerBase<T>::NextProd() const
{
    return static_cast<ProducerBase<T>*>(next_);
}


template <typename T>
size_t ProducerBase<T>::SizeApprox() const
{
    auto tail = tail_index_.load(std::memory_order_relaxed);
    auto head = head_index_.load(std::memory_order_relaxed);

    return CircularLessThan(head, tail) ? static_cast<size_t>(tail - head) : 0;
}

template <typename T>
index_t ProducerBase<T>::GetTail() const
{
    return tail_index_.load(std::memory_order_relaxed);
}


