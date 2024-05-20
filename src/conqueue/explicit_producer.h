#pragma once

#include "block.h"
#include "default_traits.h"
#include "details.h"
#include "producer_base.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <stdlib.h>
#include <block.h>

template <typename T> class ConcurrentQueue;

template <typename T>
struct ExplicitProducer : public ProducerBase<T>
{
    explicit ExplicitProducer(ConcurrentQueue<T>* parent);
    ~ExplicitProducer();

    template<AllocationMode alloc_mode, typename U>
	bool Enqueue(U&& element);

    template<typename U>
	bool Dequeue(U& element);

    template<AllocationMode alloc_mode, typename It>
	bool EnqueueBulk(It item_first, size_t count);

    template<typename It>
	size_t DequeueBulk(It& item_first, size_t max);

private:
    bool NewBlockIndex(size_t number_of_filled_slots_to_expose);
private:
    struct BlockIndexEntry
    {
    	index_t base_;
    	Block<T>* block_;
    };

    struct BlockIndexHeader
    {
    	size_t size_;
    	std::atomic<size_t> front_;		
    	BlockIndexEntry* entries_;
    	void* prev_;
    };
private:
    std::atomic<BlockIndexHeader*> block_index_;
            
    size_t block_index_slots_used_;
    size_t block_index_size_;
    size_t block_index_front_;		
    BlockIndexEntry* block_Index_entries_;
    void* block_index_raw_;
};







///////////////////////////////////////// 实现
template <typename T>
ExplicitProducer<T>::ExplicitProducer(ConcurrentQueue<T>* parent)
    : ProducerBase<T>(parent, true),
    block_index_(nullptr),
    block_index_slots_used_(0),
    block_index_size_(kExplicitInitialIndexSize >> 1),
    block_index_front_(0),
    block_Index_entries_(nullptr),
    block_index_raw_(nullptr)
{
    size_t pool_based_index_size = 
        CeilToPow2(parent->initial_block_pool_size_) >> 1;
    if (pool_based_index_size > block_index_size_)
        block_index_size_ = pool_based_index_size;

    NewBlockIndex(0);
}


template <typename T>
ExplicitProducer<T>::~ExplicitProducer()
{
    if (this->tail_block_ != nullptr)
    {
        Block<T>* half_dequeued_block = nullptr;
	    if ((this->head_index_.load(std::memory_order_relaxed) 
	    	& static_cast<index_t>(kBlockSize - 1)) != 0) 
	    {
	    	// The head's not on a block boundary, meaning a block somewhere is partially dequeued
	    	// (or the head block is the tail block and was fully dequeued, but the head/tail are still not on a boundary)
	    	size_t i = (block_index_front_ - block_index_slots_used_) 
	    		& (block_index_size_ - 1);

	    	while (CircularLessThan<index_t>(block_Index_entries_[i].base_ 
	    		+ kBlockSize, this->head_index_.load(std::memory_order_relaxed))) 
	    	{
	    		i = (i + 1) & (block_index_size_ - 1);
	    	}

	    	assert(CircularLessThan<index_t>(block_Index_entries_[i].base_, 
	    		this->head_index_.load(std::memory_order_relaxed)));

	    	half_dequeued_block = block_Index_entries_[i].block_;
	    }

        auto block = this->tail_block_;
	    do {
	    	block = block->next_;
	    	if (block->Block<T>::template IsEmpty<explicit_context>()) 
	    	{
	    		continue;
	    	}
	    	
	    	size_t i = 0;	// Offset into block
	    	if (block == half_dequeued_block) 
	    	{
	    		i = static_cast<size_t>(this->head_index_.load(std::memory_order_relaxed) 
	    			& static_cast<index_t>(kBlockSize - 1));
	    	}
	    	
	    	// Walk through all the items in the block; if this is the tail block, we need to stop when we reach the tail index
	    	auto last_valid_index = (this->tail_index_.load(std::memory_order_relaxed) 
	    		& static_cast<index_t>(kBlockSize - 1)) == 0 
	    			? kBlockSize : static_cast<size_t>(this->tail_index_.load(
	    				std::memory_order_relaxed) & static_cast<index_t>(kBlockSize - 1));

	    	while (i != kBlockSize && (block != this->tail_block_ || i != last_valid_index))
	    	{
	    		(*block)[i++]->~T();
	    	}
	    } while (block != this->tail_block_);
    }

    // Destroy all blocks that we own
    if (this->tail_block_ != nullptr) 
    {
    	auto block = this->tail_block_;
    	do {
    		auto next_block = block->next_;
    		this->parent_->AddBlockToFreeList(block);
    		block = next_block;
    	} while (block != this->tail_block_);
    }
                
    // Destroy the block indices
    auto header = static_cast<BlockIndexHeader*>(block_index_raw_);
    while (header != nullptr) 
    {
    	auto prev = static_cast<BlockIndexHeader*>(header->prev_);
    	header->~BlockIndexHeader();
    	free(header);
    	header = prev;
    }
}

template <typename T>
template<AllocationMode alloc_mode, typename U>
bool ExplicitProducer<T>::Enqueue(U&& element)
{
    index_t current_tail_index = this->tail_index_.load(std::memory_order_relaxed);
	index_t new_tail_index = 1 + current_tail_index;
    if ((current_tail_index & static_cast<index_t>(kBlockSize - 1)) == 0)
    {
        auto start_block = this->tail_block_;
        auto original_block_index_slots_used = block_index_slots_used_;
        if (this->tail_block_ != nullptr
            && this->tail_block_->next_->Block<T>::template 
                IsEmpty<explicit_context>())
        {
            this->tail_block_ = this->tail_block_->next_;
			this->tail_block_->Block<T>::template ResetEmpty<explicit_context>();
        }
        else 
        {
            auto head = this->head_index_.load(std::memory_order_relaxed);
            assert(!CircularLessThan<index_t>(current_tail_index, head));
            if (!CircularLessThan<index_t>(head, current_tail_index + kBlockSize)
			    || (kMaxSubqueueSize != ConstNumericMax<size_t>::value 
				    && (kMaxSubqueueSize == 0 || kMaxSubqueueSize - kBlockSize 
					    < current_tail_index - head))) 
            {
                return false;
            }

            if (block_index_raw_ == nullptr || block_index_slots_used_ == block_index_size_)
            {
                if (alloc_mode == CANNOT_ALLOC)
                    return false;
                else if (!NewBlockIndex(block_index_slots_used_))
                    return false;
            }

            auto new_block = this->parent_->ConcurrentQueue<T>::template RequisitionBlock<alloc_mode>();
		    if (new_block == nullptr) 
		    	return false;

            new_block->Block<T>::template ResetEmpty<explicit_context>();
            if (this->tail_block_ == nullptr)
                new_block->next_ = new_block;
            else
            {
                new_block->next_ = this->tail_block_->next_;
                this->tail_block_->next_ = new_block;
            }
            this->tail_block_ = new_block;
            ++block_index_slots_used_;
        }
        if (!noexcept(new(static_cast<T *>(nullptr)) T(std::forward<U>(element))))
        {
            try {
                new ((*this->tail_block_)[current_tail_index]) T(std::forward<U>(element));
            } 
            catch (...) 
            {
                block_index_slots_used_ = original_block_index_slots_used;
                this->tail_block_ = start_block == nullptr ? this->tail_block_ : start_block;
                throw;    
            }
        }
        else 
        {
            (void)start_block;
            (void)original_block_index_slots_used;
        }

        auto& entry = block_index_.load(std::memory_order_relaxed)->entries_[block_index_front_];
	    entry.base_ = current_tail_index;
	    entry.block_ = this->tail_block_;
	    block_index_.load(std::memory_order_relaxed)->front_.store(block_index_front_, 
	    	std::memory_order_release);
	    block_index_front_ = (block_index_front_ + 1) & (block_index_size_ - 1);

        if (!noexcept(new(static_cast<T *>(nullptr)) T(std ::forward<U>(element))))
        {
            this->tail_index_.store(new_tail_index, std::memory_order_release);
            return true;
        }
    }

    new ((*this->tail_block_)[current_tail_index]) T(std::forward<U>(element));
    this->tail_index_.store(new_tail_index, std::memory_order_release);
    return true;
}

template <typename T>
template<typename U>
bool ExplicitProducer<T>::Dequeue(U& element)
{
    auto tail = this->tail_index_.load(std::memory_order_relaxed);
    auto over_commit = this->dequeue_overcommit_.load(std::memory_order_relaxed);
    if (CircularLessThan<index_t>(this->dequeue_optimistic_count_.load(std::memory_order_relaxed)
				 - over_commit, tail))
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        auto my_dequeue_count = this->dequeue_optimistic_count_.fetch_add(1, std::memory_order_relaxed);
        tail = this->tail_index_.load(std::memory_order_acquire);

        if ((likely)(CircularLessThan<index_t>(my_dequeue_count - over_commit, tail))) 
        {
            auto index = this->head_index_.fetch_add(1, std::memory_order_acq_rel);
            auto local_block_index = block_index_.load(std::memory_order_acquire);
			auto local_block_index_head = local_block_index->front_.load(std::memory_order_acquire);

            auto head_base = local_block_index->entries_[local_block_index_head].base_;
		    auto block_base_index = index & ~static_cast<index_t>(kBlockSize - 1);
		    auto offset = static_cast<size_t>(
		    		static_cast<typename std::make_signed<index_t>::type>(block_base_index 
		    	- head_base) / static_cast<typename std::make_signed<index_t>::type>(kBlockSize));
		    auto block = local_block_index->entries_[(local_block_index_head + offset) & (local_block_index->size_ - 1)].block_;


            auto& el = *((*block)[index]);
			if (noexcept(element = std ::move(el))) 
            {
                struct Guard 
                {
			    	Block<T>* block_;
			    	index_t index_;
			    	
			    	~Guard()
			    	{
			    		(*block_)[index_]->~T();
			    		block_->Block<T>::template SetEmpty<explicit_context>(index_);
			    	}
			    } guard = { block, index };

                element = std::move(el);
            }
            else 
            {
                element = std::move(el); // NOLINT
			    el.~T(); // NOLINT
			    block->Block<T>::template SetEmpty<explicit_context>(index);
            }
            return true;
        }
        else 
        {
            this->dequeue_overcommit_.fetch_add(1, std::memory_order_release);
        }
    }

    return false;
}

template <typename T>
template<AllocationMode alloc_mode, typename It>
bool ExplicitProducer<T>::EnqueueBulk(It item_first, size_t count)
{
    index_t start_tail_index = this->head_index_.load(std::memory_order_relaxed);
    auto start_block = this->tail_block_;
    auto original_block_index_front = block_index_front_;
    auto original_block_index_slots_used = block_index_slots_used_;

    Block<T>* first_allocated_block = nullptr;
    size_t block_base_diff = ((start_tail_index + count - 1) 
        & ~static_cast<index_t>(kBlockSize - 1)) - ((start_tail_index - 1)
        & ~static_cast<index_t>(kBlockSize - 1));
    index_t current_tail_index = (start_tail_index - 1) & ~static_cast<index_t>(kBlockSize - 1);
    if (block_base_diff > 0)
    {
        while (block_base_diff > 0 && this->tail_block_ != nullptr
            && this->tail_block_->next_ != first_allocated_block
            && this->tail_block_->next_->Block<T>::template
            IsEmpty<explicit_context>())
        {
            block_base_diff -= static_cast<index_t>(kBlockSize);
			current_tail_index += static_cast<index_t>(kBlockSize);

            this->tail_block_ = this->tail_block_->next_;
			first_allocated_block = first_allocated_block == nullptr 
			    ? this->tail_block_ : first_allocated_block;
            
            auto& entry = block_index_.load(std::memory_order_relaxed)->entries_[block_index_front_];
		    entry.base_ = current_tail_index;
		    entry.block_ = this->tail_block_;
		    block_index_front_ = (block_index_front_ + 1) & (block_index_size_ - 1);
        }

        while (block_base_diff > 0)
        {
            block_base_diff -= static_cast<index_t>(kBlockSize);
            current_tail_index += static_cast<index_t>(kBlockSize);

            auto head = this->head_index_.load(std::memory_order_relaxed);
            assert(CircularLessThan<index_t>(current_tail_index, head));
            bool full = CircularLessThan<index_t>(head, current_tail_index
                + kBlockSize) || (kMaxSubqueueSize != ConstNumericMax<size_t>::value
                && (kMaxSubqueueSize == 0
                || kMaxSubqueueSize - kBlockSize < current_tail_index - head));
            if (block_index_raw_ == nullptr 
                || block_index_slots_used_ == block_index_size_ || full)
            {
                if (alloc_mode == CANNOT_ALLOC)
                {
                    block_index_front_ = original_block_index_front;
                    block_index_slots_used_ = original_block_index_slots_used;
                    this->tail_block_ = start_block == nullptr ? first_allocated_block : start_block;
                    return false;
                }
                else if (full || !NewBlockIndex(original_block_index_slots_used))
                {
                    block_index_front_ = original_block_index_front;
                    block_index_slots_used_ = original_block_index_slots_used;
                    this->tail_block_ = start_block == nullptr ? first_allocated_block : start_block;
                    return false;
                }
                original_block_index_front = original_block_index_slots_used;
            }

            auto new_block = this->parent_->ConcurrentQueue<T>::template
                RequisitionBlock<T, alloc_mode>();
            if (new_block == nullptr)
            {
                block_index_front_ = original_block_index_front;
                block_index_slots_used_ = original_block_index_slots_used;
                this->tail_block_ = start_block == nullptr ? first_allocated_block : start_block;
                return false;
            }

            new_block->ConcurrentQueue<T>::template SetAllEmpty<explicit_context>();
            if (this->tail_block_ == nullptr)
                new_block->next_ = new_block;
            else
            {
                new_block->next_ = this->tail_block_->next_;
                this->tail_block_->next_ = new_block;
            }

            this->tail_block_ = new_block;
            first_allocated_block = first_allocated_block == nullptr ? this->tail_block_ : first_allocated_block;
            ++block_index_slots_used_;

            auto& entry = block_index_.load(std::memory_order_relaxed)->entries_[block_index_front_];
            entry.base_ = current_tail_index;
            entry.block_ = this->tail_block_;
            block_index_front_ = (block_index_front_ + 1) & (block_index_size_ - 1);
        }

        auto block = first_allocated_block;
        while (true)
        {
            block->Block<T>::template ResetEmpty<explicit_context>();
            if (block == this->tail_block_)
                break;
            block = block->next_;
        }

        if (noexcept(new(static_cast<T *>(nullptr)) T(DerefNoexcept(item_first))))
        {
            block_index_.load(std::memory_order_relaxed)->front_.store(
                (block_index_front_ - 1) & (block_index_size_ - 1),
                std::memory_order_release);
        }
    }

    index_t new_tail_index = start_tail_index + static_cast<index_t>(count);
    current_tail_index = start_tail_index;
    auto end_block = this->tail_block_;
    this->tail_block_ = start_block;
    assert((start_tail_index & static_cast<index_t>(kBlockSize - 1)) != 0
        || first_allocated_block != nullptr || count);
    if ((start_tail_index & static_cast<index_t>(kBlockSize - 1)) == 0
        && first_allocated_block != nullptr)
    {
        this->tail_block_ = first_allocated_block;
    }

    while (true)
    {
        index_t stop_index = (current_tail_index & ~static_cast<index_t>(kBlockSize - 1)
            + static_cast<index_t>(kBlockSize));
        if (CircularLessThan<index_t>(new_tail_index, stop_index))
        {
            stop_index = new_tail_index;
        }

        if (noexcept(new(static_cast<T*>(nullptr)) T(DerefNoexcept(item_first))))
        {
            while (current_tail_index != stop_index)
                new ((*this->tail_block_)[current_tail_index++]) T(*item_first++);
        }
        else 
        {
            try 
            {
                while (current_tail_index != stop_index)
                {
                    new ((*this->tail_block_)[current_tail_index]) 
				        T(NoMoveIf<noexcept(new(static_cast<T *>(nullptr)) 
                            T(DerefNoexcept(item_first)))>::Eval(*item_first));
                    ++current_tail_index;
                    ++item_first;
                }
            } 
            catch (...) 
            {
                auto constructed_stop_index = current_tail_index;
                auto last_block_enqueued = this->tail_block_;

                block_index_front_ = original_block_index_front;
                block_index_slots_used_ = original_block_index_slots_used;
                this->tail_block_ = start_block == nullptr ? first_allocated_block : start_block;

                if (!IsTriviallyDestructible<T>::value)
                {
                    auto block = start_block;
                    if ((start_tail_index & static_cast<index_t>(kBlockSize - 1)) == 0)
                        block = first_allocated_block;
                    
                    current_tail_index = start_tail_index;
                    while (true)
                    {
                        stop_index = (current_tail_index & ~static_cast<index_t>(kBlockSize - 1))
                            + static_cast<index_t>(kBlockSize);
                        if (CircularLessThan<index_t>(constructed_stop_index, stop_index))
                            stop_index = constructed_stop_index;
                        
                        while (current_tail_index != stop_index)
                        {
                            (*block)[current_tail_index++]->~T();
                        }

                        if (block == last_block_enqueued)
                            break;
                        block = block->next_;
                    }
                }
                throw;
            }
        }

        if (this->tail_block_ == end_block)
        {
            assert(current_tail_index == new_tail_index);
            break;
        }
        this->tail_block_ = this->tail_block_->next_;
    }

    if (!noexcept(new(static_cast<T *>(nullptr)) T(DerefNoexcept(item_first))))
    {
        if (first_allocated_block != nullptr)
            block_index_.load(std::memory_order_relaxed)->front_.store(
            (block_index_front_ - 1) & (block_index_size_ - 1),
            std::memory_order_release);
    }

    this->tail_index_.store(new_tail_index, std::memory_order_release);
    return true;
}

template <typename T>
template<typename It>
size_t ExplicitProducer<T>::DequeueBulk(It& item_first, size_t max)
{
    auto tail = this->tail_block_.load(std::memory_order_relaxed);
    auto over_commit = this->dequeue_overcommit_.load(std::memory_order_relaxed);
    auto desired_count = static_cast<size_t>(
        tail - (this->dequeue_optimistic_count_.load(std::memory_order_relaxed) - over_commit));

    if (CircularLessThan<index_t>(0, desired_count))
    {
        desired_count = desired_count < max ? desired_count : max;
        std::atomic_thread_fence(std::memory_order_acquire);

        auto my_dequeue_count = this->dequeue_optimistic_count_.fetch_add(desired_count,
            std::memory_order_relaxed);
        tail = this->tail_index_.load(std::memory_order_acquire);
        auto actual_count = static_cast<size_t>(tail - (my_dequeue_count - over_commit));
        if (CircularLessThan<index_t>(0, actual_count)) 
        {
            actual_count = desired_count < actual_count ? desired_count : actual_count;
		    if (actual_count < desired_count) 
		    {
		    	this->dequeue_overcommit_.fetch_add(desired_count - actual_count, 
		    	std::memory_order_release);
		    }

            auto first_index = this->head_index_.fetch_add(actual_count, std::memory_order_acq_rel);

            auto local_block_index = block_index_.load(std::memory_order_acquire);
			auto local_block_index_head = local_block_index->front_.load(std::memory_order_acquire);

            auto head_base = local_block_index->entries_[local_block_index_head].base_;
		    auto first_block_base_index = first_index & ~static_cast<index_t>(kBlockSize - 1);
		    auto offset = static_cast<size_t>(static_cast<typename 
		    	std::make_signed<index_t>::type>(first_block_base_index - head_base) 
		    		/ static_cast<typename std::make_signed<index_t>::type>(kBlockSize));
		    auto indexIndex = (local_block_index_head + offset) & (local_block_index->size_ - 1);


            auto index = first_index;
            do {
			    auto first_index_in_block = index;
			    index_t end_index = (index & ~static_cast<index_t>(kBlockSize - 1)) + 
			    	static_cast<index_t>(kBlockSize);
			    end_index = CircularLessThan<index_t>(first_index + 
			    	static_cast<index_t>(actual_count), end_index)
			    	? first_index + static_cast<index_t>(actual_count) : end_index;
			    auto block = local_block_index->entries_[indexIndex].block_;
			    if (noexcept(DerefNoexcept(item_first) = std::move((*(*block)[index])))) 
			    {
			    	while (index != end_index) 
			    	{
			    		auto& el = *((*block)[index]);
			    		*item_first++ = std::move(el);
			    		el.~T();
			    		++index;
			    	}
			    }
			    else 
			    {
			    	try 
			    	{
			    		while (index != end_index) 
			    		{
			    			auto& el = *((*block)[index]);
			    			*item_first = std::move(el);
			    			++item_first;
			    			el.~T();
			    			++index;
			    		}
			    	}
			    	catch (...) 
			    	{
			    		do {
			    			block = local_block_index->entries_[indexIndex].block_;
			    			while (index != end_index) 
                            {
			    				(*block)[index++]->~T();
			    			}
			    			block->Block<T>::template 
			    				SetManyEmpty<explicit_context>(first_index_in_block, 
			    					static_cast<size_t>(end_index - first_index_in_block));
			    			indexIndex = (indexIndex + 1) & (local_block_index->size - 1);

			    			first_index_in_block = index;
			    			end_index = (index & ~static_cast<index_t>(kBlockSize - 1)) 
			    				+ static_cast<index_t>(kBlockSize);
			    			end_index = CircularLessThan<index_t>(
			    				first_index + static_cast<index_t>(actual_count), end_index) 
			    				? first_index + static_cast<index_t>(actual_count) : end_index;
			    		} while (index != first_index + actual_count);

			    		throw;
			    	}
			    }
			    block->Block<T>::template 
			    	SetManyEmpty<explicit_context>(first_index_in_block, 
			    		static_cast<size_t>(end_index - first_index_in_block));
			    indexIndex = (indexIndex + 1) & (local_block_index->size - 1);
			} while (index != first_index + actual_count);
					
			return actual_count;
        }
        else 
        {
            this->dequeue_overcommit_.fetch_add(desired_count, std::memory_order_release);
        }
    }

    return 0;
}

template <typename T>
bool ExplicitProducer<T>::NewBlockIndex(size_t number_of_filled_slots_to_expose)
{
    auto prev_block_size_mask = block_index_size_ - 1;
    block_index_size_ <<= 1;
    
    auto new_raw_ptr = static_cast<char*>(malloc(sizeof(BlockIndexHeader) 
    + std::alignment_of<BlockIndexEntry>::value - 1 + sizeof(BlockIndexEntry) 
        * block_index_size_));
    if (new_raw_ptr == nullptr)
    {
        block_index_size_ >>= 1;
        return false;
    }

    auto new_block_index_entries = reinterpret_cast<BlockIndexEntry*>(
        AlignFor<BlockIndexEntry>(new_raw_ptr + sizeof(BlockIndexHeader)));

    size_t j = 0;
    if (block_index_slots_used_ != 0)
    {
        auto i = (block_index_front_ - block_index_slots_used_) & prev_block_size_mask;
        do {
            new_block_index_entries[j++] = block_Index_entries_[i];
            i = (i + 1) & prev_block_size_mask;
        }while (i != block_index_front_);
    }

    auto header = new (new_raw_ptr) BlockIndexHeader;
    header->size_ = block_index_size_;
    header->front_.store(number_of_filled_slots_to_expose - 1, std::memory_order_relaxed);
    header->entries_ = new_block_index_entries;
    header->prev_ = new_block_index_entries;		// we link the new block to the old one so we can free it later
                
    block_index_front_ = j;
    block_Index_entries_ = new_block_index_entries;
    block_index_raw_ = new_raw_ptr;
    block_index_.store(header, std::memory_order_release);

    return true;
}

