#pragma once

#include "default_traits.h"
#include "details.h"
#include "producer_base.h"
#include "block.h"
#include <atomic>
#include <cassert>
#include <stdlib.h>

template <typename T>
class ConcurrentQueue;

template <typename T>
struct ImplicitProducer : public ProducerBase<T>
{
    ImplicitProducer(ConcurrentQueue<T>* parent_);
    ~ImplicitProducer();

// 入队操作
    template<AllocationMode alloc_mode, typename U>
    bool Enqueue(U&& element);  // 入队

    template<AllocationMode alloc_mode, typename It>
	bool EnqueueBulk(It item_first, size_t count);  // 批量入队

// 出队操作
    template<typename U>
	bool Dequeue(U& element);   // 出队

    template<typename It>
	size_t DequeueBulk(It& item_first, size_t max); // 批量出队

private:
    struct BlockIndexEntry      // 块索引条目
    {
        std::atomic<index_t> key_;      // 块索引序号
		std::atomic<Block<T>*> value_;  // 块的地址
    };

    struct BlockIndexHeader     // 块索引头
    {
        size_t capacity_;           // BlockIndexEntry的个数
	    std::atomic<size_t> tail_;  // entries_的末尾元素
	    BlockIndexEntry* entries_;  // 存储BlockIndexEntry对象,是个数组
	    BlockIndexEntry** index_;   // 存储entires_中每个条目的地址
	    BlockIndexHeader* prev_;    // 指向上一次分配的BlockIndex
    };
private:

    /**
    * @brief 插入一个BlockIndex条目,将参数block_start_index插入进去,
    *         并返回这块索引地址,返回值为参数index_entry
    * @param index_entry 输出值,指向BlockIndex数组的某个块索引
    * @param block_start_index 输入值,
    * @return true 插入成功
    * @return false 插入失败
    */
    template<AllocationMode allocMode>
	bool InsertBlockIndexEntry(BlockIndexEntry*& index_entry, index_t block_start_index);

    /**
    * @brief 回滚块索引尾部,将尾部向前移动一个位置
    * 
    */
    void RewindBlockIndexTail();

    /**
     * @brief 
     * 
     * @param index 
     * @return BlockIndexEntry* 
     */
    BlockIndexEntry* GetBlockIndexEntryForIndex(index_t index) const;

    /**
     * @brief 
     * 
     * @param index 
     * @param local_block_index 
     * @return size_t 
     */
    size_t GetBlockIndexIndexForIndex(index_t index, BlockIndexHeader*& local_block_index) const;

    /**
     * @brief 创建一个块索引,内存布局为: [BlockIndexHeader + entires + 存储entires每个条目地址]
     * 
     * @return true 
     * @return false 
     */
    bool NewBlockIndex();
private:
    size_t next_block_index_capacity_;              // 需要要分配块大小的容量
    std::atomic<BlockIndexHeader*> block_index_;    // block索引,指向一个block索引头
    static const index_t kInvalidBlockBase = 1;
};







///////////////////////////////////// 实现
template <typename T>
ImplicitProducer<T>::ImplicitProducer(ConcurrentQueue<T>* parent)
    : ProducerBase<T>(parent, false),
    next_block_index_capacity_(kImplicitInitialIndexSize),
    block_index_(nullptr)
{ NewBlockIndex(); }

template <typename T>
ImplicitProducer<T>::~ImplicitProducer()
{
    auto tail = this->tail_index_.load(std::memory_order_relaxed);
    auto index = this->head_index_.load(std::memory_order_relaxed);
    Block<T>* block = nullptr;
    assert(index == tail || CircularLessThan(index, tail));
    bool force_free_last_block = index != tail;		// If we enter the loop, then the last (tail) block will not be freed
    while (index != tail) 
    {
    	if ((index & static_cast<index_t>(kBlockSize - 1)) == 0 || block == nullptr) 
        {
    		if (block != nullptr) 
            {
    			this->parent_->AddBlockToFreeList(block);
    		}
    		
    		block = GetBlockIndexEntryForIndex(index)->value_.load(std::memory_order_relaxed);
    	}
    	
    	((*block)[index])->~T();
    	++index;
    }

    if (this->tail_block_ != nullptr && (force_free_last_block 
        || (tail & static_cast<index_t>(kBlockSize - 1)) != 0)) 
    {
	    this->parent_->AddBlockToFreeList(this->tail_block_);
	}

    auto local_block_index = block_index_.load(std::memory_order_relaxed);
    if (local_block_index != nullptr) 
    {
    	for (size_t i = 0; i != local_block_index->capacity_; ++i) 
    		local_block_index->index_[i]->~BlockIndexEntry();

    	do {
    		auto prev = local_block_index->prev_;
    		local_block_index->~BlockIndexHeader();
    		free(local_block_index);
    		local_block_index = prev;
    	} while (local_block_index != nullptr);
    }
}

template <typename T>
template<AllocationMode alloc_mode, typename U>
/**
 * @brief 数据入队操作.
 * 
 * @param element 
 * @return true 
 * @return false 
 */
bool ImplicitProducer<T>::Enqueue(U&& element)
{
    index_t current_tail_index = this->tail_index_.load(std::memory_order_relaxed);
	index_t new_tail_index = 1 + current_tail_index;    // 下一次插入的块索引序号

    // 限制tail_index_最大为kBlockSize - 1
    if ((current_tail_index & static_cast<index_t>(kBlockSize - 1)) == 0) 
    {
	    // 我们到达了一个区块的末尾，开始一个新的区块
	    auto head = this->head_index_.load(std::memory_order_relaxed);
	    assert(!CircularLessThan<index_t>(current_tail_index, head));
	    if (!CircularLessThan<index_t>(head, current_tail_index + kBlockSize) 
            || (kMaxSubqueueSize != ConstNumericMax<size_t>::value 
            && (kMaxSubqueueSize == 0 
            || kMaxSubqueueSize - kBlockSize < current_tail_index - head))) 
        {
	    	return false;
	    }

        BlockIndexEntry* index_entry;
	    if (!InsertBlockIndexEntry<alloc_mode>(index_entry, current_tail_index)) 
        {
	    	return false;
	    }

        auto new_block = this->parent_->ConcurrentQueue<T>::template 
            RequisitionBlock<alloc_mode>(); // 申请一个块

        //Block<T>* new_block; // 测试 TODO
        if (new_block == nullptr)
        {
            RewindBlockIndexTail();
            index_entry->value_.store(nullptr, std::memory_order_relaxed);
            return false;
        }

        new_block->Block<T>::template ResetEmpty<implicit_context>();

        // 检测new运算符创建对象是否会抛出异常
        if (!noexcept(new(static_cast<T*>(nullptr)) T(std::forward<U>(element))))   
        {
            // 进入到这里表示“会”抛出异常
            try 
            {
                new ((*new_block)[current_tail_index]) T(std::forward<U>(element));
            } 
            catch (...) 
            {
                RewindBlockIndexTail();
                index_entry->value_.store(nullptr, std::memory_order_relaxed);
                this->parent_->AddBlockToFreeList(new_block);
                throw;
            }
        }

        index_entry->value_.store(new_block, std::memory_order_relaxed);
        this->tail_block_ = new_block;

        if (!noexcept(new(static_cast<T *>(nullptr)) T(std ::forward<U>(element))))
        {
            this->tail_index_.store(new_tail_index, std::memory_order_release);
            return true;
        }
    }
    // 在new_block上构造传入进来的元素
    new ((*this->tail_block_)[current_tail_index]) T(std::forward<U>(element));
	// 下一次再默认插入元素的下标
    this->tail_index_.store(new_tail_index, std::memory_order_release);
    return true;
}

template <typename T>
template<typename U>
bool ImplicitProducer<T>::Dequeue(U& element)
{
    index_t tail = this->tail_index_.load(std::memory_order_relaxed);
    index_t over_commit = this->dequeue_overcommit_.load(std::memory_order_relaxed);

    // 乐观出队次数 - 出队失败的次数 >= tail块索引末尾序号的话,那么就表示没有元素可以出队了
    if (CircularLessThan<index_t>(this->dequeue_optimistic_count_.load(std::memory_order_relaxed) - over_commit, tail)) 
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        
        // 乐观出队计数器加1
        index_t my_dequeue_count = this->dequeue_optimistic_count_.fetch_add(1, std::memory_order_relaxed);
        tail = this->tail_index_.load(std::memory_order_acquire);

        // 用于告诉编译器提示代码中的分支预向,可以提高编译器优化
        if ((likely)(CircularLessThan<index_t>(my_dequeue_count - over_commit, tail))) 
        {
        	index_t index = this->head_index_.fetch_add(1, std::memory_order_acq_rel);
        	auto entry = GetBlockIndexEntryForIndex(index); // 获取一个块索引

            // 块索引的value指向一个块
        	auto block = entry->value_.load(std::memory_order_relaxed);
        	auto& el = *((*block)[index]);  // 获取block里面的值

            if (!noexcept(element = std::move(el))) // 如果将el移动到参数element会抛出异常的
            {
                struct Guard 
                {
                    ~Guard()
                    {
                        (*block_)[index_]->~T();    // 将block里面的值析构

                        // 如果SetEmpty返回true,表示该块是个空块
					    if (block_->Block<T>::template SetEmpty<implicit_context>(index_)) 
                        {
                            // 将这个块从块索引中删除
					    	entry_->value_.store(nullptr, std::memory_order_relaxed);
					    	parent_->AddBlockToFreeList(block_);    // 将该空块加入空闲链表中
					    }
                    }

                    Block<T>* block_;
				    index_t index_; 
				    BlockIndexEntry* entry_;
				    ConcurrentQueue<T>* parent_;
                } guard = { block, index, entry, this->parent_ };

                element = std::move(el);
            }
            else 
            {
                element = std::move(el);
                el.~T();    // 原插入的值进行析构

                // 如果SetEmpty返回true,表示该块是个空块
                if (block->Block<T>::template SetEmpty<implicit_context>(index))
                {
                    {
                        // 将这个块从块索引中删除
                        entry->value_.store(nullptr, std::memory_order_relaxed);
                    }
                    this->parent_->AddBlockToFreeList(block); // 将该空块加入空闲链表中
                }
            }
            return true;
        }
        else 
        {
            // 记录一次尝试出队但失败的次数
            this->dequeue_overcommit_.fetch_add(1, std::memory_order_release);
        }
    }
    return false;
}

template <typename T>
template<AllocationMode allocMode, typename It>
bool ImplicitProducer<T>::EnqueueBulk(It item_first, size_t count)
{
    index_t start_tail_index = this->tail_index_.load(std::memory_order_relaxed);
    auto start_block = this->tail_block_;
    Block<T>* first_allocated_block = nullptr;
    auto end_block = this->tail_block_;

    size_t block_base_diff = ((start_tail_index + count - 1) & ~static_cast<index_t>(kBlockSize - 1)) 
        - ((start_tail_index - 1) & ~static_cast<index_t>(kBlockSize - 1));
    index_t current_tail_index = (start_tail_index - 1) & ~static_cast<index_t>(kBlockSize - 1);
    if (block_base_diff > 0) 
    {
        do {
		    block_base_diff -= static_cast<index_t>(kBlockSize);
		    current_tail_index += static_cast<index_t>(kBlockSize);
		    
		    // Find out where we'll be inserting this block in the block index
		    BlockIndexEntry* index_entry = nullptr;  // initialization here unnecessary but compiler can't always tell
		    Block<T>* new_block;
		    bool index_inserted = false;
		    auto head = this->head_Index_.load(std::memory_order_relaxed);
		    assert(!CircularLessThan<index_t>(current_tail_index, head));
		    bool full = !CircularLessThan<index_t>(head, current_tail_index + kBlockSize) 
                || (kMaxSubqueueSize != ConstNumericMax<size_t>::value 
                && (kMaxSubqueueSize == 0 
                || kMaxSubqueueSize - kBlockSize < current_tail_index - head));

		    if (full || !(index_inserted = InsertBlockIndexEntry<allocMode>(index_entry, current_tail_index)) 
                || (new_block = this->parent_->ConcurrentQueue<T>::template 
                RequisitionBlock<allocMode>()) == nullptr) 
            {
		    	// Index allocation or block allocation failed; revert any other allocations
		    	// and index insertions done so far for this operation
		    	if (index_inserted) 
                {
		    		RewindBlockIndexTail();
		    		index_entry->value_.store(nullptr, std::memory_order_relaxed);
		    	}
		    	current_tail_index = (start_tail_index - 1) & ~static_cast<index_t>(kBlockSize - 1);
		    	for (auto block = first_allocated_block; block != nullptr; block = block->next_) 
                {
		    		current_tail_index += static_cast<index_t>(kBlockSize);
		    		index_entry = GetBlockIndexEntryForIndex(current_tail_index);
		    		index_entry->value_.store(nullptr, std::memory_order_relaxed);
		    		RewindBlockIndexTail();
		    	}
		    	this->parent_->add_blocks_to_free_list(first_allocated_block);
		    	this->tail_block_ = start_block;
		    	
		    	return false;
		    }

            new_block->Block<T>::template ResetEmpty<implicit_context>();
			new_block->next_ = nullptr;

            index_entry->value_.store(new_block, std::memory_order_relaxed);
            if ((start_tail_index & static_cast<index_t>(kBlockSize - 1)) != 0 
                || first_allocated_block != nullptr) 
            {
				assert(this->tail_block_ != nullptr);
				this->tail_block_->next_ = new_block;
			}
            this->tail_block_ = new_block;
	    	end_block = new_block;
	    	first_allocated_block = first_allocated_block == nullptr ? new_block : first_allocated_block;
	    } while (block_base_diff > 0);
    }

    index_t new_tail_index = start_tail_index + static_cast<index_t>(count);
    current_tail_index = start_tail_index;
    this->tail_block_ = start_block;
    assert((start_tail_index & static_cast<index_t>(kBlockSize - 1)) != 0 || first_allocated_block != nullptr || count == 0);
    if ((start_tail_index & static_cast<index_t>(kBlockSize - 1)) == 0 
        && first_allocated_block != nullptr) 
    {
    	this->tail_block_ = first_allocated_block;
    }
    while (true) 
    {
    	index_t stopIndex = (current_tail_index & ~static_cast<index_t>(kBlockSize - 1)) + static_cast<index_t>(kBlockSize);
    	if (CircularLessThan<index_t>(new_tail_index, stopIndex)) 
        {
    		stopIndex = new_tail_index;
    	}

    	if (noexcept(new(static_cast<T *>(nullptr)) T(DerefNoexcept(item_first)))) 
        {
    		while (current_tail_index != stopIndex) 
            {
    			new ((*this->tail_block_)[current_tail_index++]) T(*item_first++);
    		}
    	}
    	else 
        {
    		try 
            {
    			while (current_tail_index != stopIndex) 
                {
                    new ((*this->tail_block_)[current_tail_index]) 
                        T(NoMoveIf<!noexcept(new(static_cast<T *>(nullptr)) 
                            T(DerefNoexcept(item_first)))>::Eval(*item_first));
    				++current_tail_index;
    				++item_first;
    			}
    		}
    		catch (...) 
            {
    			auto constructedStopIndex = current_tail_index;
    			auto lastBlockEnqueued = this->tail_block_;
    			
    			if (!IsTriviallyDestructible<T>::value) 
                {
    				auto block = start_block;
    				if ((start_tail_index & static_cast<index_t>(kBlockSize - 1)) == 0) 
    					block = first_allocated_block;
                    
    				current_tail_index = start_tail_index;
    				while (true) 
                    {
    					stopIndex = (current_tail_index & ~static_cast<index_t>(kBlockSize - 1)) + static_cast<index_t>(kBlockSize);
    					if (CircularLessThan<index_t>(constructedStopIndex, stopIndex)) 
    						stopIndex = constructedStopIndex;
                        
    					while (current_tail_index != stopIndex) 
    						(*block)[current_tail_index++]->~T();
                        
    					if (block == lastBlockEnqueued) 
    						break;
                        
    					block = block->next_;
    				}
    			}
    			
    			current_tail_index = (start_tail_index - 1) & ~static_cast<index_t>(kBlockSize - 1);
    			for (auto block = first_allocated_block; block != nullptr; block = block->next_) 
                {
    				current_tail_index += static_cast<index_t>(kBlockSize);
    				auto index_entry = GetBlockIndexEntryForIndex(current_tail_index);
    				index_entry->value_.store(nullptr, std::memory_order_relaxed);
    				RewindBlockIndexTail();
    			}
    			this->parent_->add_blocks_to_free_list(first_allocated_block);
    			this->tail_block_ = start_block;
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
    this->tail_index_.store(new_tail_index, std::memory_order_release);
    return true;
}

template <typename T>
template<typename It>
size_t ImplicitProducer<T>::DequeueBulk(It& item_first, size_t max)
{
    auto tail = this->tail_index_.load(std::memory_order_relaxed);
    auto over_commit = this->dequeue_overcommit_.load(std::memory_order_relaxed);
    auto desired_count = static_cast<size_t>(tail - (this->dequeue_optimistic_count_.load(std::memory_order_relaxed) - over_commit));
    if (CircularLessThan<size_t>(0, desired_count)) 
    {
    	desired_count = desired_count < max ? desired_count : max;
    	std::atomic_thread_fence(std::memory_order_acquire);
    	
    	auto my_dequeue_count = this->dequeue_optimistic_count_.fetch_add(desired_count, std::memory_order_relaxed);
    	
    	tail = this->tail_index_.load(std::memory_order_acquire);
    	auto actual_count = static_cast<size_t>(tail - (my_dequeue_count - over_commit));
    	if (CircularLessThan<size_t>(0, actual_count)) 
        {
    		actual_count = desired_count < actual_count ? desired_count : actual_count;
    		if (actual_count < desired_count)
    			this->dequeue_overcommit_.fetch_add(desired_count - actual_count, std::memory_order_release);
    		
    		// Get the first index. Note that since there's guaranteed to be at least actual_count elements, this
    		// will never exceed tail.
    		auto first_index = this->head_index_.fetch_add(actual_count, std::memory_order_acq_rel);
    		
    		// Iterate the blocks and dequeue
    		auto index = first_index;
    		BlockIndexHeader* local_block_index;
    		auto index_index = GetBlockIndexIndexForIndex(index, local_block_index);
    		do {
    			auto block_start_index = index;
    			index_t end_index = (index & ~static_cast<index_t>(kBlockSize - 1)) + static_cast<index_t>(kBlockSize);
    			end_index = CircularLessThan<index_t>(first_index + static_cast<index_t>(actual_count), end_index) 
                    ? first_index + static_cast<index_t>(actual_count) : end_index;
    			
    			auto entry = local_block_index->index_[index_index];
    			auto block = entry->value_.load(std::memory_order_relaxed);
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
    					do 
                        {
    						entry = local_block_index->index_[index_index];
    						block = entry->value_.load(std::memory_order_relaxed);
    						while (index != end_index) 
    							(*block)[index++]->~T();
    						
    						if (block->Block<T>::template SetManyEmpty<implicit_context>(block_start_index, static_cast<size_t>(
                                end_index - block_start_index))) 
                            {

    							entry->value_.store(nullptr, std::memory_order_relaxed);
    							this->parent_->AddBlockToFreeList(block);
    						}
    						index_index = (index_index + 1) & (local_block_index->capacity_ - 1);
    						
    						block_start_index = index;
    						end_index = (index & ~static_cast<index_t>(kBlockSize - 1)) + static_cast<index_t>(kBlockSize);
    						end_index = CircularLessThan<index_t>(first_index + static_cast<index_t>(actual_count), end_index) 
                                ? first_index + static_cast<index_t>(actual_count) : end_index;
    					} while (index != first_index + actual_count);
    					
    					throw;
    				}
    			}
    			if (block->Block<T>::template SetManyEmpty<implicit_context>(block_start_index, 
                    static_cast<size_t>(end_index - block_start_index))) 
                {
    		        {
    		        	entry->value_.store(nullptr, std::memory_order_relaxed);
    		        }
    		        this->parent_->AddBlockToFreeList(block);		// releases the above store
    			}
    			index_index = (index_index + 1) & (local_block_index->capacity_ - 1);
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


/**
 * @brief 插入一个块索引,参数block_start_index作为索引的key
 *         并返回这块索引地址,返回值为参数index_entry
 * @param index_entry 输出参数,指向BlockIndex数组的某个块索引
 * @param block_start_index 输入参数
 * @return true 插入成功
 * @return false 插入失败
 */
template <typename T>
template<AllocationMode alloc_mode>
bool ImplicitProducer<T>::InsertBlockIndexEntry(BlockIndexEntry*& index_entry, index_t block_start_index)
{
    auto local_block_index = block_index_.load(std::memory_order_relaxed);		// 是唯一写入的,所以relaxed内存模型也是可以的
    if (local_block_index == nullptr)
    	return false;  // NewBlockIndex()函数执行失败,内部创建堆内存失败,这里会执行到
    
    // 设置一个新的尾,指向BlockIndexEntry数组
    size_t new_tail = (local_block_index->tail_.load(std::memory_order_relaxed) + 1) & (local_block_index->capacity_ - 1);
    index_entry = local_block_index->index_[new_tail];  // 设置输出值,指向BlockIndex数组具体块索引
    if (index_entry->key_.load(std::memory_order_relaxed) == kInvalidBlockBase
        || index_entry->value_.load(std::memory_order_relaxed) == nullptr) // 表示这个块索引没有被用过
    { 
        // 那么设置其内容, key_为参数 block_start_index
    	index_entry->key_.store(block_start_index, std::memory_order_relaxed);
    	local_block_index->tail_.store(new_tail, std::memory_order_release);
    	return true;
    }

    // 执行到这里,表示这个块索引已经被用过了



    if (alloc_mode == CANNOT_ALLOC) // 如果分配模式是不能分配,那么则退出
    	return false;
    else if (!NewBlockIndex())  // 分配模式为可以分配,则重新分配一个块索引数组
    	return false;   // 分配失败则退出
    else 
    {
    	local_block_index = block_index_.load(std::memory_order_relaxed);
        // 设置新的尾指向块索引数组
    	new_tail = (local_block_index->tail_.load(std::memory_order_relaxed) + 1) & (local_block_index->capacity_ - 1);
    	index_entry = local_block_index->index_[new_tail];  // 设置输出值,指向BlockIndex数组具体块索引
        // 断言,判断key是否被使用过了
    	assert(index_entry->key_.load(std::memory_order_relaxed) == kInvalidBlockBase);

        // 然后设置内容并返回
    	index_entry->key_.store(block_start_index, std::memory_order_relaxed);
    	local_block_index->tail_.store(new_tail, std::memory_order_release); 
    	return true;
    }
}

/**
 * @brief 回滚块索引尾部,将尾部向前移动一个位置
 * 
 */
template <typename T>
void ImplicitProducer<T>::RewindBlockIndexTail()
{
    auto local_block_index = block_index_.load(std::memory_order_relaxed);
	local_block_index->tail_.store((local_block_index->tail_.load(std::memory_order_relaxed) - 1) 
        & (local_block_index->capacity_ - 1), std::memory_order_relaxed);
}


/**
 * @brief 
 * 
 * @param index 
 * @return ImplicitProducer<T>::BlockIndexEntry* 
 */
template <typename T>
typename ImplicitProducer<T>::BlockIndexEntry* 
    ImplicitProducer<T>::GetBlockIndexEntryForIndex(index_t index) const
{
    BlockIndexHeader* local_block_index;
    auto idx = GetBlockIndexIndexForIndex(index, local_block_index);
    return local_block_index->index_[idx];
}



/**
 * @brief 
 * 
 * @param index 输入参数
 * @param local_block_index 输出参数,
 * @return size_t 
 */
template <typename T>
size_t ImplicitProducer<T>::GetBlockIndexIndexForIndex(index_t index, BlockIndexHeader*& local_block_index) const
{
    index &= ~static_cast<index_t>(kBlockSize - 1);
    local_block_index = block_index_.load(std::memory_order_acquire);
    auto tail = local_block_index->tail_.load(std::memory_order_acquire);
    auto tail_base = local_block_index->index_[tail]->key_.load(std::memory_order_relaxed);
    assert(tail_base != kInvalidBlockBase);
    // Note: 必须使用除法而不是移位，因为索引可能会循环，导致负偏移，我们希望保留其负值
    auto offset = static_cast<size_t>(static_cast<typename std::make_signed<index_t>::type>(index - tail_base) 
        / static_cast<typename std::make_signed<index_t>::type>(kBlockSize));
    size_t idx = (tail + offset) & (local_block_index->capacity_ - 1);
    assert(local_block_index->index_[idx]->key_.load(std::memory_order_relaxed) ==   
        local_block_index->index_[idx]->key_.load(std::memory_order_relaxed));
    return idx;
}






/**
 * @brief 分配一个 [BlockIndexHeader + entires + 存储entires每个条目地址]这样的内存,
 *          而且保证这个内存足够大
 * 
 * @return true 
 * @return false 
 */
template <typename T>
bool ImplicitProducer<T>::NewBlockIndex()
{
    auto prev = block_index_.load(std::memory_order_relaxed);
    size_t prev_capacity = prev == nullptr ? 0 : prev->capacity_;
    auto entry_count = prev == nullptr ? next_block_index_capacity_ : prev_capacity;

    // TODO: 两次减1反而不能让内存对齐,不能提高内存效率,看看是否可以改
    // 分配一个堆内存,用来存储 [BlockIndexHeader + entires + 存储entires每个条目地址]这样的数据
    auto raw = static_cast<char*>(malloc(sizeof(BlockIndexHeader) + 
        std::alignment_of<BlockIndexEntry>::value - 1 + sizeof(BlockIndexEntry) * entry_count
        + std::alignment_of<BlockIndexEntry*>::value - 1 + sizeof(BlockIndexEntry*) 
        * next_block_index_capacity_));

    if (raw == nullptr)
        return false;
    
    auto header = new (raw) BlockIndexHeader;   // 构造一个BlockIndexHeader
    // 调整指针位置,指向存储条目项的位置
    auto entries = reinterpret_cast<BlockIndexEntry*>(AlignFor<BlockIndexEntry>(raw + sizeof(BlockIndexHeader)));
    // 调整指针位置,指向存储每个条目地址的位置
    auto index = reinterpret_cast<BlockIndexEntry**>(AlignFor<BlockIndexEntry*>(reinterpret_cast<char*>(entries) 
        + sizeof(BlockIndexEntry) * entry_count));

    if (prev != nullptr)    // 再次之前已经分配过BlockIndex数组了
    {
        // 获取之前BlockIndex的条目数组中的最后一个
        auto prev_tail = prev->tail_.load(std::memory_order_relaxed);
        auto prev_pos = prev_tail;
        size_t i = 0;

        // 将之前BlockIndex中存储条目地址的数组拷贝到当前新分配BlockIndex中的index_中
        do {
            prev_pos = (prev_pos + 1) & (prev->capacity_ - 1);
            index[i++] = prev->index_[prev_pos];
        } while (prev_pos != prev_tail);
        assert(i == prev_capacity);
    }

    // 初始化entries(底层对象是BlockIndexEntry),并在内存最后存储每个条目的地址
    for (size_t i = 0; i != entry_count; i++)   
    {
        new (entries + i) BlockIndexEntry;  // 创建BlockIndexEntry
        entries[i].key_.store(kInvalidBlockBase, std::memory_order_relaxed);
        index[prev_capacity + i] = entries + i; // 存储每个条目的地址
    }

    // BlockIndexHeader设置有关信息
    header->prev_ = prev;       // 指向上一次分配的BlockIndex
    header->entries_ = entries; // 指向条目数组
    header->index_ = index;     // 指向存储条目地址的数组
    header->capacity_ = next_block_index_capacity_; // 条目个数, 默认32
    // tail_默认为 next_block_index_capacity_ - 1
    header->tail_.store((prev_capacity - 1) & (next_block_index_capacity_ - 1), std::memory_order_relaxed);

    block_index_.store(header, std::memory_order_release);
    next_block_index_capacity_ <<= 1;   // 下一次要分配块内存所需要的大小

    return true;
}




