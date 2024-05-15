#pragma once

#include "details.h"

template <typename T> class ConcurrentQueue;

template<typename T> class BlockingConcurrentQueue;

struct ConcurrentQueueProducerTypelessBase;

struct ProducerToken
{
    template<typename T>
	explicit ProducerToken(ConcurrentQueue<T>& queue);

    template<typename T>
	explicit ProducerToken(BlockingConcurrentQueue<T>& queue);
    ~ProducerToken();

    ProducerToken(ProducerToken&& other) noexcept;
    inline ProducerToken& operator=(ProducerToken&& other) noexcept;

    ProducerToken(ProducerToken const&) = delete;
    ProducerToken& operator=(ProducerToken const&) = delete;

    void Swap(ProducerToken& other) noexcept;
    bool Valid();
private:
    template<typename T>
    friend class ConcurrentQueue;

    friend class ConcurrentQueueTests;
protected:
    ConcurrentQueueProducerTypelessBase* producer_;
};




////////////////////////////////////////////// 实现
template<typename T>
    ProducerToken::ProducerToken(ConcurrentQueue<T>& queue)
    : producer_(queue.RecycleOrCreateProducer(true))
{
    if (producer_ != nullptr)
        producer_->token_ = this;
}

template<typename T>
ProducerToken::ProducerToken(BlockingConcurrentQueue<T>& queue)
    : producer_(reinterpret_cast<ConcurrentQueue<T>*>(&queue)->RecycleOrCreateProducer(true))
{
    if (producer_ != nullptr)
        producer_->token_ = this;
}