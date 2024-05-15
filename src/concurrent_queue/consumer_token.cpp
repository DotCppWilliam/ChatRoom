#include "consumer_token.h"

ConsumerToken::ConsumerToken(ConsumerToken&& other) noexcept
    : initial_offset_(other.initial_offset_),
    last_known_global_offset_(other.last_known_global_offset_),
    items_consumed_from_current_(other.items_consumed_from_current_),
    current_producer_(other.current_producer_),
    desired_producer_(other.desired_producer_)
{

}

ConsumerToken& ConsumerToken::operator=(ConsumerToken&& other) noexcept
{
    Swap(other);
    return *this;
}


void ConsumerToken::Swap(ConsumerToken& other) noexcept
{
    std::swap(initial_offset_, other.initial_offset_);
    std::swap(last_known_global_offset_, other.last_known_global_offset_);
    std::swap(items_consumed_from_current_, other.items_consumed_from_current_);
    std::swap(current_producer_, other.current_producer_);
    std::swap(desired_producer_, other.desired_producer_);
}

