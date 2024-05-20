#pragma once

#include "details.h"
#include "implicit_producer.h"

#include <atomic>

template <typename T>
struct ImplicitProducerKVP
{
    ImplicitProducerKVP();
    ~ImplicitProducerKVP() {}
    ImplicitProducerKVP(ImplicitProducerKVP&& other) noexcept;
    ImplicitProducerKVP& operator=(ImplicitProducerKVP&& other) noexcept;
    void Swap(ImplicitProducerKVP& other) noexcept;

    std::atomic<ThreadId_t> key_;   // 存储线程id的地址
    ImplicitProducer<T>* value_;    // 存储这个线程对应着的隐式生产者
};



template <typename T>
struct ImplicitProducerHash
{
    ImplicitProducerHash()
        : capacity_(0), entries_(nullptr), prev_(nullptr) { }
    ~ImplicitProducerHash() {}
	size_t capacity_;                   // 哈希的容量
	ImplicitProducerKVP<T>* entries_;   // 条目
	ImplicitProducerHash* prev_;        // 指向上一次已分配的隐式生产者哈希
};









///////////////////////////////////////////////// 实现
template <typename T>
ImplicitProducerKVP<T>::ImplicitProducerKVP()
    : value_(nullptr), key_(0)
{

}

template <typename T>
ImplicitProducerKVP<T>::ImplicitProducerKVP(ImplicitProducerKVP&& other) noexcept
{
    key_.store(other.key_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    value_ = other.value_;
}

template <typename T>
ImplicitProducerKVP<T>& 
    ImplicitProducerKVP<T>::operator=(ImplicitProducerKVP&& other) noexcept
{
    Swap(other);
    return *this;
}

template <typename T>
void ImplicitProducerKVP<T>::Swap(ImplicitProducerKVP& other) noexcept
{
    if (this != &other)
    {
        SwapRelaxed(key_, other.key_);
        std::swap(value_, other.value_);
    }
}
