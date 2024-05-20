#include "producer_token.h"
#include <atomic>

ProducerToken::~ProducerToken()
{
    if (producer_ != nullptr)
    {
        producer_->token_ = nullptr;
        producer_->inactive_.store(true, std::memory_order_release);
    }
}

ProducerToken::ProducerToken(ProducerToken&& other) noexcept
{
    other.producer_ = nullptr;
    if (producer_ != nullptr)
        producer_->token_ = this;   
}

ProducerToken& ProducerToken::operator=(ProducerToken&& other) noexcept
{   
    Swap(other);
    return *this;
}   

void ProducerToken::Swap(ProducerToken& other) noexcept
{
    std::swap(producer_, other.producer_);
    if (producer_ != nullptr)
        producer_->token_ = this;

    if (other.producer_ != nullptr)
        other.producer_->token_ = &other;
}

bool ProducerToken::Valid()
{ return producer_ != nullptr; }

