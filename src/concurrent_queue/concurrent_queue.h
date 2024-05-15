#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include "details.h"
#include "default_traits.h"

template <typename T>struct ExplicitProducer;
template <typename T> struct ImplicitProducer;
template <typename T> struct ProducerBase;
template <typename T> struct Block;
template <typename Node> struct FreeList;
template <typename T> struct ImplicitProducerHash;
template <typename T> struct ImplicitProducerKVP;

struct ProducerToken;
struct ConsumerToken;

template <typename T>
class ConcurrentQueue
{
    friend struct ProducerToken;
    friend struct ConsumerToken;
    friend struct ExplicitProducer<T>;
    friend struct ImplicitProducer<T>;
    friend class ConcurrentQueueTests;
public:
    using index_t = size_t;
public:
    /**
     * @brief 构造一个并发队列
     * 
     * @param capacity 默认32 * 32 = 1024 
     */
    explicit ConcurrentQueue(size_t capacity = 32 *  kBlockSize);   
    ConcurrentQueue(size_t min_capacity, size_t max_explicit_producers,
        size_t max_implicit_producers);
    ~ConcurrentQueue();

    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue operator=(ConcurrentQueue const&) = delete;
    ConcurrentQueue(ConcurrentQueue&& other) noexcept;
    ConcurrentQueue& operator=(ConcurrentQueue&& other) noexcept;
public:
    void Swap(ConcurrentQueue& other) noexcept;
    size_t SizeApprox() const;          // 大约尺寸
    static constexpr bool IsLockFree(); // 是否是无锁的

// 入队操作
    bool Enqueue(T const& item);
    bool Enqueue(T&& item);
    bool Enqueue(ProducerToken const& token, const T& item);
    bool Enqueue(ProducerToken const& token, T&& item);
    
    template <typename It>
    bool EnqueueBulk(It item_first, size_t count);  // 批量

    template <typename It>
    bool EnqueueBulk(ProducerToken const& token, It item_first, size_t count);  // 批量

    bool TryEnqueue(T const& item);
    bool TryEnqueue(T&& item);
    bool TryEnqueue(ProducerToken const& token, T const& item);
    bool TryEnqueue(ProducerToken const& token, T&& item);

    template <typename It>
    bool TryEnqueueBulk(It item_first, size_t count);

    template <typename It>
    bool TryEnqueueBulk(ProducerToken const& token, It item_first, size_t count);

// 出队操作
    template <typename U>
    bool TryDequeue(U& item);

    template <typename U>
    bool TryDequeue(ConsumerToken& token, U& item);

    template <typename U>
    bool TryDequeueNonInterleaved(U& item);

    template <typename It>
    size_t TryDequeueBulk(ConsumerToken& token, It item_first, size_t max);

    template <typename It>
    size_t TryDequeueBulk(It item_first, size_t max);

    template <typename U>
    bool TryDequeueFromProducer(ProducerToken const& producer, U& item);

    template <typename It>
    size_t TryDequeueBulkFromProducer(ProducerToken const& producer,
        It item_first, size_t max);
public:
    /**
     * @brief 填充初始阻塞列表
     * 
     * @param block_count 
     */
    void PopulateInitialBlockList(size_t block_count);

    /**
     * @brief 尝试从初始池中获取块
     * 
     * @return Block<T>* 
     */
    inline Block<T>* TryGetBlockFromInitialPool();

    /**
     * @brief 将块添加到空闲列表
     * 
     * @param block 
     */
    inline void AddBlockToFreeList(Block<T>* block);

    /**
     * @brief 将一些块添加到空闲列表中
     * 
     * @param block 
     */
    inline void AddBlocksToFreeList(Block<T>* block);

    /**
     * @brief 尝试从空闲块中获取块
     * 
     * @return Block<T>* 
     */
    inline Block<T>* TryGetBlockFromFreeList();

    /**
     * @brief 申请块
     * 
     * @tparam can_alloc 
     * @return Block<T>* 
     */
    template <AllocationMode can_alloc>
    Block<T>* RequisitionBlock();

    /**
     * @brief 回收或创建生产者
     * 
     * @param is_explicit 
     * @return ProducerBase<T>* 
     */
    ProducerBase<T>* RecycleOrCreateProducer(bool is_explicit);

    /**
     * @brief 添加生产者
     * 
     * @param producer 
     * @return ProducerBase<T>* 
     */
    ProducerBase<T>* AddProducer(ProducerBase<T>* producer);

    /**
     * @brief 
     * 
     */
    void ReownProducers();
private:
    /**
     * @brief 交换内部空间
     * 
     * @param other 
     * @return ConcurrentQueue& 
     */
    ConcurrentQueue& SwapInternal(ConcurrentQueue& other);

    /**
     * @brief 
     * 
     * @tparam can_alloc 
     * @tparam U 
     * @param token 
     * @param element 
     * @return true 
     * @return false 
     */
    template<AllocationMode can_alloc, typename U>
    bool InnerEnqueue(ProducerToken const& token, U&& element);

    /**
     * @brief 
     * 
     * @tparam can_alloc 
     * @tparam U 
     * @param element 
     * @return true 
     * @return false 
     */
    template<AllocationMode can_alloc, typename U>
    bool InnerEnqueue(U&& element);

    /**
     * @brief 
     * 
     * @tparam can_alloc 
     * @tparam It 
     * @param token 
     * @param itemFirst 
     * @param count 
     * @return true 
     * @return false 
     */
    template<AllocationMode can_alloc, typename It>
    bool InnerEnqueueBulk(ProducerToken const& token, It itemFirst, size_t count);

    /**
     * @brief 
     * 
     * @tparam can_alloc 
     * @tparam It 
     * @param itemFirst 
     * @param count 
     * @return true 
     * @return false 
     */
    template<AllocationMode can_alloc, typename It>
    bool InnerEnqueueBulk(It itemFirst, size_t count);

    /**
     * @brief 轮换后更新当前生产者
     * 
     * @param token 
     * @return true 
     * @return false 
     */
    bool UpdateCurrentProducerAfterRotation(ConsumerToken& token);
    
    /**
     * @brief 填充初始隐式生产者哈希
     * 
     */
    void PopulateInitialImplicitProducerHash();

    /**
     * @brief 交换隐式生产者哈希值
     * 
     * @param other 
     */
    void SwapImplicitProducerHashes(ConcurrentQueue<T>& other);

    /**
     * @brief 获取或添加隐式生产者
     * 
     * @return ImplicitProducer<T>* 
     */
    ImplicitProducer<T>* GetOrAddImplicitProducer();
private:
    std::atomic<ProducerBase<T>*> producer_list_tail_;  // 生产者列表尾部
    std::atomic<std::uint32_t> producer_count_;         // 生产者个数
    std::atomic<size_t> initial_block_pool_index_;      // 初始块池索引
    Block<T>* initial_block_pool_;                      // 初始块池
    size_t initial_block_pool_size_;                    // 初始块池大小

    FreeList<Block<T>> free_list_;                                          // 空闲链表
    std::atomic<ImplicitProducerHash<T>*> implicit_producer_hash_;          // 隐式生产者哈希
    std::atomic<size_t> implicit_producer_hash_count_;                      // 隐式生产者哈希个数
    ImplicitProducerHash<T> initial_implicit_producer_hash_;                // 初始隐式生产者哈希
    std::array<ImplicitProducerKVP<T>,  kInitialImplicitProducerHashSize>   
        initial_implicit_producer_hash_entries_;                            // 初始隐式生产者哈希条目
    std::atomic_flag implicit_producer_hash_resize_in_progress_;            // 隐式生产者哈希大小调整正在进行中
    std::atomic<std::uint32_t> next_explicit_consumer_id_;                  // 下一个显式消费者id
    std::atomic<std::uint32_t> global_explicit_consumer_offset_;            // 全局显式消费者偏移值
};
