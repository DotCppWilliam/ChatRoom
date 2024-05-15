#pragma once

#include "concurrent_queue.h"

#include "consumer_token.h"
#include "block.h"
#include "details.h"
#include "free_list.h"
#include "producer_token.h"
#include "implicit_producer.h"
#include "explicit_producer.h"
#include "producer_base.h"
#include "implicit_producer_hash.h"
#include "utility_functions.h"

///////////////////////////////////////// 实现
/**
 * @brief 构造一个并发队列,给定初始容量.
 *        主要完成任务: 初始化一个块池和隐式生产者哈希
 * 
 * @param capacity 
 */
template <typename T>
ConcurrentQueue<T>::ConcurrentQueue(size_t capacity)
    : producer_list_tail_(nullptr),
    producer_count_(0),
    initial_block_pool_index_(0),
    next_explicit_consumer_id_(0),
    global_explicit_consumer_offset_(0)
{
    implicit_producer_hash_resize_in_progress_.clear(std::memory_order_relaxed);
    PopulateInitialImplicitProducerHash();
    PopulateInitialBlockList(capacity /  kBlockSize + 
        ((capacity & (kBlockSize - 1)) == 0 ? 0 : 1));  /*
                                                        这里kBlockSize - 1的作用是(capacity / kBlockSize)的余数
                                                        要多分配一块,比如capacity = 7, kBlockSize = 4
                                                        capacity & (kBlockSize - 1) = 7 & (4 - 1) = 3,然后这个3需要多分配一块
                                                        */
}

/**
 * @brief 构造一个并发队列
 * 
 * @param min_capacity 队列最小容量
 * @param max_explicit_producers 显式生产者最大个数
 * @param max_implicit_producers 隐式生产者最大个数
 */
template <typename T>
ConcurrentQueue<T>::ConcurrentQueue(size_t min_capacity, 
    size_t max_explicit_producers, size_t max_implicit_producers)
    : producer_list_tail_(nullptr),
    producer_count_(0),
    initial_block_pool_index_(0),
    next_explicit_consumer_id_(0),
    global_explicit_consumer_offset_(0)
{
    implicit_producer_hash_resize_in_progress_.clear(std::memory_order_relaxed);
    PopulateInitialImplicitProducerHash();

    /*
        1. (min_capacity + kBlockSize - 1) / kBlockSize： 用来计算以 kBlockSize 为单位对齐后的 min_capacity 所需要的块数
            确保计算结果肯定为偶数
        然后再此基础上再减去1,可能是要保留一个块作为初始化块
        2. (max_explicit_producers + 1): 为每个显式生产者预留足够的块数
        3. 2 * (max_explicit_producers + max_implicit_producers): 额外的预留空间,防止频繁分配内存
        4. 将1和2相乘,显式生产者需要的块数
    */
    size_t blocks = (((min_capacity +  kBlockSize - 1) /  kBlockSize) - 1)
        * (max_explicit_producers + 1) + 2 * (max_explicit_producers + max_implicit_producers);
    PopulateInitialBlockList(blocks);
}

template <typename T>
ConcurrentQueue<T>::~ConcurrentQueue()
{
    auto ptr = producer_list_tail_.load(std::memory_order_relaxed);
    while (ptr != nullptr)
    {
        auto next = ptr->NextProd();
        if (ptr->token_ != nullptr)
            ptr->token_->producer_ = nullptr;

        Destroy(ptr);
        ptr = next;
    }

    if (kInitialImplicitProducerHashSize != 0)
    {
        auto hash = implicit_producer_hash_.load(std::memory_order_relaxed);
        while (hash != nullptr)
        {
            auto prev = hash->prev_;
            if (prev != nullptr)
            {
                for (size_t i = 0; i != hash->capacity_; i++)
                    hash->entries_[i].~ImplicitProducerKVP();
                hash->~ImplicitProducerHash();
                free(hash);
            }
            hash = prev;
        }
    }

    auto block = free_list_.HeadUnsafe();
    while (block != nullptr)
    {
        auto next = block->free_list_next_.load(std::memory_order_relaxed);
        if (block->dynamically_allocated_)
            Destroy(block);
        block = next;
    }

    DestroyArray(initial_block_pool_, initial_block_pool_size_);
}

template <typename T>
ConcurrentQueue<T>::ConcurrentQueue(ConcurrentQueue&& other) noexcept
    : producer_list_tail_(other.producer_list_tail.load(std::memory_order_relaxed)),
    producer_count_(other.producer_count_.load(std::memory_order_relaxed)),
    initial_block_pool_index_(other.initial_block_pool_index_.load(std::memory_order_relaxed)),
    initial_block_pool_(other.initial_block_pool_),
    initial_block_pool_size_(other.initial_block_pool_size_),
    free_list_(std::move(other.free_list_)),
    next_explicit_consumer_id_(other.next_explicit_consumer_id_.load(std::memory_order_relaxed)),
    global_explicit_consumer_offset_(other.global_explicit_consumer_offset_.load(std::memory_order_relaxed))
{
    implicit_producer_hash_resize_in_progress_.clear(std::memory_order_relaxed);
    PopulateInitialImplicitProducerHash();
    SwapImplicitProducerHashes(other);

    other.producer_list_tail_.store(nullptr, std::memory_order_relaxed);
    other.producer_count_.store(0, std::memory_order_relaxed);
	other.next_explicit_consumer_id_.store(0, std::memory_order_relaxed);
	other.global_explicit_consumer_offset_.store(0, std::memory_order_relaxed);

    other.initial_block_pool_index_.store(0, std::memory_order_relaxed);
	other.initial_block_pool_size_ = 0;
	other.initial_block_pool_ = nullptr;

    ReownProducers();
}

template <typename T>
ConcurrentQueue<T>& 
    ConcurrentQueue<T>::operator=(ConcurrentQueue&& other) noexcept
{
    return SwapInternal(other);
}



template <typename T>
void ConcurrentQueue<T>::Swap(ConcurrentQueue& other) noexcept
{
    SwapInternal(other);
}

template <typename T>
bool ConcurrentQueue<T>::Enqueue(T const& item)
{
    if (kInitialImplicitProducerHashSize == 0)
        return false;
    else 
        return InnerEnqueue<CAN_ALLOC>(item);
}

template <typename T>
bool ConcurrentQueue<T>::Enqueue(T&& item)
{
    if (kInitialImplicitProducerHashSize == 0)
        return false;
    else 
        return InnerEnqueue<CAN_ALLOC>(std::move(item));
}

template <typename T>
bool ConcurrentQueue<T>::Enqueue(ProducerToken const& token, const T& item)
{
    return InnerEnqueue<CAN_ALLOC>(token, item);
}

template <typename T>
bool ConcurrentQueue<T>::Enqueue(ProducerToken const& token, T&& item)
{
    return InnerEnqueue<CAN_ALLOC>(token, std::move(item));
}

template <typename T>
template <typename It>
bool ConcurrentQueue<T>::EnqueueBulk(It item_first, size_t count)
{
    if ( kInitialImplicitProducerHashSize == 0)
        return false;
    else 
        return InnerEnqueueBulk<CAN_ALLOC>(item_first, count);
}


template <typename T>
template <typename It>
bool ConcurrentQueue<T>::EnqueueBulk(ProducerToken const& token, 
    It item_first, size_t count)
{
    return InnerEnqueueBulk<CAN_ALLOC>(token, item_first, count);
}


template <typename T>
bool ConcurrentQueue<T>::TryEnqueue(T const& item)
{
    if ( kInitialImplicitProducerHashSize == 0)
        return false;
    else
        return InnerEnqueue<CANNOT_ALLOC>(item);
}

template <typename T>
bool ConcurrentQueue<T>::TryEnqueue(T&& item)
{
    if ( kInitialImplicitProducerHashSize == 0)
        return false;
    else
        return InnerEnqueue<CANNOT_ALLOC>(std::move(item));
}

template <typename T>
bool ConcurrentQueue<T>::TryEnqueue(ProducerToken const& token, T const& item)
{
    return InnerEnqueue<CANNOT_ALLOC>(token, item);
}

template <typename T>
bool ConcurrentQueue<T>::TryEnqueue(ProducerToken const& token, T&& item)
{
    return InnerEnqueue<CANNOT_ALLOC>(token, std::move(item));
}


template <typename T>
template <typename It>
bool ConcurrentQueue<T>::TryEnqueueBulk(It item_first, size_t count)
{
    if ( kInitialImplicitProducerHashSize == 0)
        return false;
    else
        return InnerEnqueue<CANNOT_ALLOC>(item_first, count);
}

template <typename T>
template <typename It>
bool ConcurrentQueue<T>::TryEnqueueBulk(ProducerToken const& token, It item_first, size_t count)
{
    return InnerEnqueueBulk<CANNOT_ALLOC>(token, item_first, count);
}

template <typename T>
template <typename U>
bool ConcurrentQueue<T>::TryDequeue(U& item)
{
    size_t non_empty_count = 0;
    ProducerBase<T>* best = nullptr;
    size_t best_size = 0;
    for (auto ptr = producer_list_tail_.load(std::memory_order_acquire);
        non_empty_count < 3 && ptr != nullptr; ptr = ptr->NextProd())
    {
        auto size = ptr->SizeApprox();
        if (size > 0)
        {
            if (size > best_size)
            {
                best_size = size;
                best = ptr;
            }
            ++non_empty_count;
        }
    }

    if (non_empty_count > 0)
    {
        if (likely(best->Dequeue(item)))
            return true;

        for (auto ptr = producer_list_tail_.load(std::memory_order_acquire);
            ptr != nullptr; ptr = ptr->NextProd())
        {
            if (ptr != best && ptr->Dequeue(item))
                return true;
        }
    }
    return false;
}


template <typename T>
template <typename U>
bool ConcurrentQueue<T>::TryDequeue(ConsumerToken& token, U& item)
{
    if (token.desired_producer_ == nullptr || token.last_known_global_offset_
        != global_explicit_consumer_offset_.load(std::memory_order_relaxed))
    {
        if (!UpdateCurrentProducerAfterRotation(token))
            return false;
    }

    if (static_cast<ProducerBase<T>*>(token.current_producer_)->Dequeue(item))
    {
        if (++token.items_consumed_from_current_ ==  kExplicitConsumerConsumptionQuotaBeforeRotate)
        {
            global_explicit_consumer_offset_.fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }

    auto tail = producer_list_tail_.load(std::memory_order_acquire);
    auto ptr = static_cast<ProducerBase<T>*>(token.current_producer_)->NextProd();
    if (ptr == nullptr)
        ptr = tail;

    while (ptr != static_cast<ProducerBase<T>*>(token.current_producer_))
    {
        if (ptr->Dequeue(item))
        {
            token.current_producer_ = ptr;
            token.items_consumed_from_current_ = 1;
            return true;
        }
        ptr = ptr->NextProd();
        if (ptr == nullptr)
            ptr = tail;
    }
    return false;
}

template <typename T>
template <typename U>
bool ConcurrentQueue<T>::TryDequeueNonInterleaved(U& item)
{
    for (auto ptr = producer_list_tail_.load(std::memory_order_acquire);
        ptr != nullptr; ptr= ptr->NextProd())
    {
        if (ptr->Dequeue(item))
            return true;
    }
    return false;
}

template <typename T>
template <typename It>
size_t ConcurrentQueue<T>::TryDequeueBulk(It item_first, size_t max)
{
    size_t count = 0;
    for (auto ptr = producer_list_tail_.load(std::memory_order_acquire); 
        ptr != nullptr; ptr = ptr->NextProd())
    {
        count += ptr->DequeueBulk(item_first, max - count);
        if (count == max)
            break;
    }

    return count;
}

template <typename T>
template <typename It>
size_t ConcurrentQueue<T>::TryDequeueBulk(ConsumerToken& token, It item_first, 
    size_t max)
{
    if (token.desired_producer_ == nullptr || token.last_known_global_offset_
        != global_explicit_consumer_offset_.load(std::memory_order_relaxed))
    {
        if (!UpdateCurrentProducerAfterRotation(token))
            return 0;
    }

    size_t count = static_cast<ProducerBase<T>*>(token.current_producer_)->DequeueBulk(item_first, max);
    if (count == max)
    {
        if ((token.items_consumed_from_current_ += static_cast<std::uint32_t>(max))
            >=  kExplicitConsumerConsumptionQuotaBeforeRotate)
        {
            global_explicit_consumer_offset_.fetch_add(1, std::memory_order_relaxed);
        }
        return max;
    }

    token.items_consumed_from_current_ += static_cast<std::uint32_t>(count);
    max -= count;

    auto tail = producer_list_tail_.load(std::memory_order_acquire);
    auto ptr = static_cast<ProducerBase<T>*>(token.current_producer_)->NextProd();
    if (ptr == nullptr)
    {
        ptr = tail;
    }
    while (ptr != static_cast<ProducerBase<T>*>(token.current_producer_))
    {
        auto dequeued = ptr->DequeueBulk(item_first, max);
        count += dequeued;
        if (dequeued != 0)
        {
            token.current_producer_ = ptr;
            token.items_consumed_from_current_ = static_cast<std::uint32_t>(dequeued);
        }

        if (dequeued == max)
            break;
        max -= dequeued;
        ptr = ptr->NextProd();
        if (ptr == nullptr)
            ptr = tail;
    }
    return count;
}

template <typename T>
template <typename U>
bool ConcurrentQueue<T>::TryDequeueFromProducer(ProducerToken const& producer, U& item)
{
    return static_cast<ExplicitProducer<T>*>(producer.producer_)->Dequeue(item);
}

template <typename T>
template <typename It>
size_t ConcurrentQueue<T>::TryDequeueBulkFromProducer(ProducerToken const& producer,
        It item_first, size_t max)
{
    return static_cast<ExplicitProducer<T>*>(producer.producer_)->dequeue_bulk(item_first, max);
}

template <typename T>
size_t ConcurrentQueue<T>::SizeApprox() const
{
    size_t size = 0;
    for (auto ptr = producer_list_tail_.load(std::memory_order_acquire); 
        ptr != nullptr; ptr = ptr->next_prod()) 
    {
    	size += ptr->size_approx();
    }
    return size;
}

template <typename T>
constexpr bool ConcurrentQueue<T>::IsLockFree()
{
    return
        StaticIsLockFree<bool>::value == 2 &&
        StaticIsLockFree<size_t>::value == 2 &&
        StaticIsLockFree<std::uint32_t>::value == 2 &&
        StaticIsLockFree<index_t>::value == 2 &&
        StaticIsLockFree<void*>::value == 2 &&
        StaticIsLockFree<typename ThreadIdConverter<ThreadId_t>::ThreadIdNumericSize_t>::value == 2;
}

template <typename T>
ConcurrentQueue<T>& ConcurrentQueue<T>::SwapInternal(ConcurrentQueue<T>& other)
{
    if (this == &other) 
    	return *this;
    SwapRelaxed(producer_list_tail_, other.producer_list_tail_);
    SwapRelaxed(producer_count_, other.producer_count_);
    SwapRelaxed(initial_block_pool_index_, other.initial_block_pool_index_);

    std::swap(initial_block_pool_, other.initial_block_pool_);
    std::swap(initial_block_pool_size_, other.initial_block_pool_size_);

    free_list_.Swap(other.free_list_);
    SwapRelaxed(next_explicit_consumer_id_, other.next_explicit_consumer_id_);
    SwapRelaxed(global_explicit_consumer_offset_, other.global_explicit_consumer_offset_);

    SwapImplicitProducerHashes(other);

    ReownProducers();
    other.ReownProducers();

    return *this;
}


template <typename T>
bool ConcurrentQueue<T>::UpdateCurrentProducerAfterRotation(ConsumerToken& token)
{
    auto tail = producer_list_tail_.load(std::memory_order_acquire);
    if (token.desired_producer_ == nullptr && tail == nullptr)
        return false;

    auto prod_count = producer_count_.load(std::memory_order_relaxed);
    auto global_offset = global_explicit_consumer_offset_.load(std::memory_order_relaxed);
    if (unlikely(token.desired_producer_ == nullptr))
    {
        std::uint32_t offset = prod_count - 1 - (token.initial_offset_ % prod_count);
        token.desired_producer_ = tail;
        for (std::uint32_t i = 0; i != offset; i++)
        {
            token.desired_producer_ = 
                static_cast<ProducerBase<T>>(token.desired_producer_).NextProd();
            if (token.desired_producer_ == nullptr)
                token.desired_producer_ = tail;
        }
    }

    std::uint32_t delta = global_offset - token.last_known_global_offset_;
    if (delta >= prod_count)
        delta = delta % prod_count;
    
    for (std::uint32_t i = 0; i != delta; i++)
    {
        token.desired_producer_ = 
            static_cast<ProducerBase<T>*>(token.desired_producer_)->NextProd();
        if (token.desired_producer_ == nullptr)
            token.desired_producer_ = tail;
    }

    token.last_known_global_offset_ = global_offset;
    token.current_producer_ = token.desired_producer_;
    token.items_consumed_from_current_ = 0;

    return true;
}


/**
 * @brief 填充初始阻塞列表
 * 
 * @param block_count 列表中的块个数
 */
template <typename T>
void ConcurrentQueue<T>::PopulateInitialBlockList(size_t block_count)
{
    initial_block_pool_size_ = block_count;
    if (initial_block_pool_size_ == 0)
    {
        initial_block_pool_ = nullptr;
        return;
    }

    // 创建块池,大小为block_count
    initial_block_pool_ = CreateArray<Block<T>>(block_count);
    if (initial_block_pool_ == nullptr)
        initial_block_pool_size_ = 0;
    
    for (size_t i = 0; i < initial_block_pool_size_; i++)
        initial_block_pool_[i].dynamically_allocated_ = false;
}

template <typename T>
ProducerBase<T>* ConcurrentQueue<T>::RecycleOrCreateProducer(bool is_explicit)
{
    for (auto ptr = producer_list_tail_.load(std::memory_order_acquire);
        ptr != nullptr; ptr = ptr->NextProd())  // 遍历生产者链表,是否有可以回收的
    {
        if (ptr->inactive_.load(std::memory_order_relaxed) 
            && ptr->is_explicit_ == is_explicit)    // 如果可回收
        {
            bool expected = true;
            if (ptr->inactive_.compare_exchange_strong(expected, false, std::memory_order_acquire,
                std::memory_order_relaxed))
            {
                return ptr; // 则回收该生产者并返回
            }
        }
    }
    return AddProducer(is_explicit ? static_cast<ProducerBase<T>*>(Create<ExplicitProducer<T>>(this)) 
        : Create<ImplicitProducer<T>>(this));
}

template <typename T>
ProducerBase<T>* ConcurrentQueue<T>::AddProducer(ProducerBase<T>* producer)
{
    if (producer == nullptr)   
        return nullptr;
    producer_count_.fetch_add(1, std::memory_order_relaxed);

    auto prev_tail = producer_list_tail_.load(std::memory_order_relaxed);
    do {
        producer->next_ = prev_tail;
    } while (!producer_list_tail_.compare_exchange_weak(prev_tail, producer, 
        std::memory_order_release, std::memory_order_relaxed));

    return producer;
}

template <typename T>
void ConcurrentQueue<T>::ReownProducers()
{
    for (auto ptr = producer_list_tail_.load(std::memory_order_relaxed); 
        ptr != nullptr; ptr = ptr->NextProd())
    {
        ptr->parent_ = this;
    }
}


/**
 * @brief 填充初始隐式生产者哈希
 * 
 */
template <typename T>
void ConcurrentQueue<T>::PopulateInitialImplicitProducerHash()
{
    if (kInitialImplicitProducerHashSize == 0)
        return;
    else
    {
        implicit_producer_hash_count_.store(0, std::memory_order_relaxed);
        auto hash = &initial_implicit_producer_hash_;
        hash->capacity_ = kInitialImplicitProducerHashSize;
        hash->entries_ = &initial_implicit_producer_hash_entries_[0];
        for (size_t i = 0; i != kInitialImplicitProducerHashSize; i++)
            initial_implicit_producer_hash_entries_[i].key_.store(kInvalidThreadId, std::memory_order_relaxed); 

        hash->prev_ = nullptr;
        implicit_producer_hash_.store(hash, std::memory_order_relaxed);
    }
}


template <typename T>
void ConcurrentQueue<T>::SwapImplicitProducerHashes(ConcurrentQueue<T>& other)
{
    if (kInitialImplicitProducerHashSize == 0)
        return;
    else
    {
        initial_implicit_producer_hash_entries_.swap(other.initial_implicit_producer_hash_entries_);
        initial_implicit_producer_hash_.entries_ = &initial_implicit_producer_hash_entries_[0];
        other.initial_implicit_producer_hash_.entries_ = &other.initial_implicit_producer_hash_entries_[0];

        SwapRelaxed(implicit_producer_hash_count_, other.implicit_producer_hash_count_);
        SwapRelaxed(implicit_producer_hash_, other.implicit_producer_hash_);
        
        if (implicit_producer_hash_.load(std::memory_order_relaxed) == &other.initial_implicit_producer_hash_)
            implicit_producer_hash_.store(&initial_implicit_producer_hash_, std::memory_order_relaxed);
        else
        {
            ImplicitProducerHash<T>* hash;
            for (hash = implicit_producer_hash_.load(std::memory_order_relaxed);
                hash->prev_ != &other.initial_implicit_producer_hash_; hash = hash->prev_)
            {
                continue;
            }
            hash->prev_ = &initial_implicit_producer_hash_;
        }

        if (other.implicit_producer_hash_.load(std::memory_order_relaxed) == &initial_implicit_producer_hash_)
            other.implicit_producer_hash_.store(&other.initial_implicit_producer_hash_, std::memory_order_relaxed);
        else
        {
            ImplicitProducerHash<T>* hash;
            for (hash = other.implicit_producer_hash_.load(std::memory_order_relaxed);
                hash->prev_ != &initial_implicit_producer_hash_; hash = hash->prev_)
            {
                continue;
            }

            hash->prev_ = &other.initial_implicit_producer_hash_;
        }
    }
}


/**
 * @brief 如果当前线程在哈希中已经存储过内容那么则获取.否则创建新的对象放到哈希里面
 * 
 * @return ImplicitProducer<T>* 隐式生产者
 */
template <typename T>
ImplicitProducer<T>* ConcurrentQueue<T>::GetOrAddImplicitProducer()
{
    auto id = ThreadId();           // 获取每个线程独有的id变量地址
    auto hash_id = HashThreadId(id);   // 将这个地址转换成hash值

    auto main_hash = implicit_producer_hash_.load(std::memory_order_acquire);
    assert(main_hash != nullptr);   // 如果这里不通过,表示队列未完成初始化

    for (auto hash = main_hash; hash != nullptr; hash = hash->prev_)
    {
        auto index = hash_id;
        while (true)
        {
            index &= hash->capacity_ - 1u;  // 使用线性探测法解决哈希冲突
            // 获取当前线程在哈希中存储的key
            auto probed_key = hash->entries_[index].key_.load(std::memory_order_relaxed);
            if (probed_key == id)   // 条件成立,则表示当前线程在之前向hash存储过值
            {
                auto value = hash->entries_[index].value_;  // 获取当前线程所属的隐式生产者对象
                if (hash != main_hash)  // 表示是在之前分配的哈希里面
                {
                    // 下面是将之前哈希里面当前线程存储的值,重新哈希到当前的主哈希里面
                    index = hash_id;
                    while (true)
                    {
                        index &= main_hash->capacity_ - 1u; // 还是使用线性探测法
                        auto empty = kInvalidThreadId;

                        if (main_hash->entries_[index].key_.compare_exchange_strong(empty, id, 
                            std::memory_order_seq_cst, std::memory_order_relaxed))
                        {
                            main_hash->entries_[index].value_ = value;
                            break;
                        }
                        ++index;    // 还是使用线性探测法
                    }
                }
                return value;
            }
            if (probed_key == kInvalidThreadId) // 当前线程还没有创建任何生产者存储到hash里面
                break;
            ++index;    // 如果有哈希冲突,则指向下一个位置来判断是否设置了值
        }
    }

    auto new_count = 1 + implicit_producer_hash_count_.fetch_add(1, std::memory_order_relaxed);
    // 加一个死循环，预防其他线程正在调整哈希大小.所以当前要等待到其他线程调整完成
    while (true)
    {
        if (new_count >= (main_hash->capacity_ >> 1) && /* 如果new_count大于等于哈希总容量的二分之一,并且隐式哈希没有调整大小 */
            !implicit_producer_hash_resize_in_progress_.test_and_set(std::memory_order_acquire))
        {
            main_hash = implicit_producer_hash_.load(std::memory_order_acquire);    // 重新获取一遍
            if (new_count >= (main_hash->capacity_ >> 1))   // 再次判断是否小于哈希的总容量
            {
                size_t new_capacity = main_hash->capacity_ << 1;    // 新哈希容量为原来的2倍
                while (new_count >= (new_capacity >> 1))    // 如果new_count比新分配容量的2倍还要大,那就继续2倍扩大
                    new_capacity <<= 1;
                // 分配一块新的内存,作为存储隐式生产者哈希
                auto raw = static_cast<char*>(malloc(sizeof(ImplicitProducerHash<T>)
                    + std::alignment_of<ImplicitProducerKVP<T>>::value - 1 
                    + sizeof(ImplicitProducerKVP<T>) * new_capacity));
                if (raw == nullptr) // 分配内存失败,代表没有足够的内存可用
                {   // 恢复分配内存之前的哈希个数
                    implicit_producer_hash_count_.fetch_sub(1, std::memory_order_relaxed);
                    implicit_producer_hash_resize_in_progress_.clear(std::memory_order_relaxed);    // 清空哈希重新分配的原子标志
                    return nullptr; // 这个时候真正退出函数 <---
                }

                auto new_hash = new (raw) ImplicitProducerHash<T>();        // 构造一个隐式生产者哈希
                new_hash->capacity_ = static_cast<size_t>(new_capacity);    // 存储新哈希的容量大小

                // 将new_hash->entries_指向隐式生产者KVP数组的首地址,在隐式生产者哈希后面
                new_hash->entries_ = reinterpret_cast<ImplicitProducerKVP<T>*>(AlignFor<ImplicitProducerKVP<T>>(
                    raw + sizeof(ImplicitProducerHash<T>)));
                // 在隐式生产者哈希后面构造一个隐式生产者KVP对象数组
                for (size_t i = 0; i != new_capacity; i++)
                {
                    new (new_hash->entries_ + i) ImplicitProducerKVP<T>;
                    // 将每个隐式生产者KVP的key设置为kInvalidThreadId
                    new_hash->entries_[i].key_.store(kInvalidThreadId, std::memory_order_relaxed);
                }

                new_hash->prev_ = main_hash;    // 新哈希指向旧的哈希
                // 当前哈希存储为新哈希
                implicit_producer_hash_.store(new_hash, std::memory_order_release);
                implicit_producer_hash_resize_in_progress_.clear(std::memory_order_release);    // 将哈希重新设置大小标志清除
                main_hash = new_hash;  
            }
            else 
            {
                // 清空哈希重新分配的原子标志
                implicit_producer_hash_resize_in_progress_.clear(std::memory_order_release);
            }
        }
        // 走到这里，表示new_count比哈希容量2分之1小,然后执行下面的逻辑

        // new_count小于hash总容量的4分之3大小
        if (new_count < (main_hash->capacity_ >> 1) + (main_hash->capacity_ >> 2))  
        {
            // 回收或者创建生产者
            auto producer = static_cast<ImplicitProducer<T>*>(RecycleOrCreateProducer(false));
            if (producer == nullptr)    // 回收和创建失败
            {
                // 恢复为之前的哈希元素个数
                implicit_producer_hash_count_.fetch_sub(1, std::memory_order_relaxed);
                return nullptr;
            }
            
            auto index = hash_id;
            // 下面使用线性探测法解决哈希冲突, 缺点是导致哈希表聚集,即一些位置会比其他位置更容易产生冲突
            // TODO: 这段可以进行优化,比如使用链地址法、Cuckoo Hashing
            while (true)
            {
                index &= main_hash->capacity_ - 1u;
                auto empty = kInvalidThreadId;

                // 将当前线程id存储到对应着哈希的key
                if (main_hash->entries_[index].key_.compare_exchange_strong(empty, id, 
                    std::memory_order_seq_cst, std::memory_order_relaxed))
                {
                    main_hash->entries_[index].value_ = producer;   // 然后将value存储为当前创建的生产者对象
                    break;  // 退出循环
                }
                ++index;    // 走到这里表示有哈希冲突,则将哈希值+1,看下一个是否设置了值
            }
            return producer;
        }
        main_hash = implicit_producer_hash_.load(std::memory_order_acquire);
    }
}





template <typename T>
template<AllocationMode can_alloc, typename U>
bool ConcurrentQueue<T>::InnerEnqueue(ProducerToken const& token, U&& element)
{
    return static_cast<ExplicitProducer<T>*>(token.producer_)->
        ExplicitProducer<T>::template Enqueue<can_alloc>(
            std::forward<U>(element));
}

template <typename T>
template<AllocationMode can_alloc, typename U>
bool ConcurrentQueue<T>::InnerEnqueue(U&& element)
{
    auto producer = GetOrAddImplicitProducer(); // 获取或创建一个隐式生产者
    return producer == nullptr ? false
        : producer->ImplicitProducer<T>::template Enqueue<can_alloc>(
            std::forward<U>(element));
}

template <typename T>
template<AllocationMode can_alloc, typename It>
bool ConcurrentQueue<T>::InnerEnqueueBulk(ProducerToken const& token, It item_first, size_t count)
{
    return static_cast<ExplicitProducer<T>*>(
            token.producer_)->ExplicitProducer<T>::template 
            EnqueueBulk<can_alloc>(item_first, count);
}   

template <typename T>
template<AllocationMode can_alloc, typename It>
bool ConcurrentQueue<T>::InnerEnqueueBulk(It item_first, size_t count)
{
    auto producer = GetOrAddImplicitProducer();
    return producer == nullptr ? false
        : producer->ImplicitProducer<T>::template EnqueueBulk<can_alloc>(
            item_first, count);
}



template <typename T>
Block<T>* ConcurrentQueue<T>::TryGetBlockFromInitialPool()
{
    if (initial_block_pool_index_.load(std::memory_order_relaxed) >= initial_block_pool_size_)
        return nullptr;
    auto index = initial_block_pool_index_.fetch_add(1, std::memory_order_relaxed);
    return index < initial_block_pool_size_ ? (initial_block_pool_ + index) : nullptr;
}

template <typename T>
void ConcurrentQueue<T>::AddBlockToFreeList(Block<T>* block)
{
    if (!kRecycleAllocatedBlocks && block->dynamically_allocated_)
        Destroy(block);
    else
        free_list_.Add(block);
}

template <typename T>
inline void ConcurrentQueue<T>::AddBlocksToFreeList(Block<T>* block)
{
    while (block != nullptr)
    {
        auto next = block->next_;
        AddBlockToFreeList(block);
        block = next;
    }
}

template <typename T>
inline Block<T>* ConcurrentQueue<T>::TryGetBlockFromFreeList()
{ return free_list_.TryGet();  }


template <typename T>
template <AllocationMode can_alloc>
Block<T>* ConcurrentQueue<T>::RequisitionBlock()
{
    auto block = TryGetBlockFromInitialPool();  // 从块池中获取一个块
    if (block != nullptr)
        return block;
    block = TryGetBlockFromFreeList();  // 从空闲块链表中获取一个块
    if (block != nullptr)
        return block;
    if (can_alloc == CAN_ALLOC)     // 如果空闲块链表也没有了,那么则创建一个并返回
        return Create<Block<T>>();
    else
        return nullptr;             // 不允许分配的话那最终返回nullptr
}











template <typename T>
inline void Swap(ConcurrentQueue<T>& a, ConcurrentQueue<T>& b) noexcept
{ a.Swap(b); }


template <typename T>
inline void Swap(ProducerToken& a, ProducerToken& b) noexcept
{ a.Swap(b); }

template <typename T>
inline void Swap(ConsumerToken& a, ConsumerToken& b) noexcept
{ a.Swap(b); }

template <typename T>
inline void Swap(ImplicitProducerKVP<T>& a, ImplicitProducerKVP<T>& b) noexcept
{ a.Swap(b); }




