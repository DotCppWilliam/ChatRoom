#pragma once

#include "details.h"

#include <cstddef>
#include <cstdint>



using index_t = size_t;

/*
    在内部，所有元素都从多元素块中入队和出队；这是最小的可控单元。
    如果你预期有少量元素但有很多生产者，则应该选择较小的块大小。
    对于少量生产者和/或很多元素，则更倾向于较大的块大小。提供了一个合理的默认值。必须是2的幂
*/
static const size_t kBlockSize = 32;

/*
    对于显式的生产者（即使用生产者令牌时），通过遍历一个元素对应一个标志的列表来检查块是否为空。
    对于较大的块大小，这种方式效率太低，而使用基于原子计数器的方法更快速。
    当块大小严格大于此阈值时，将切换到基于原子计数器的方法。
 */
static const size_t kExplicitBlockEmptyCounterThreshold = 32;

// 单个显式生产者可以期望有多少个完整块？这应该反映出该数字的最大值，以实现最佳性能。必须是2的幂
static const size_t kExplicitInitialIndexSize = 32;

// 单个隐式生产者可以期望有多少个完整块？这应该反映出该数字的最大值，以实现最佳性能。必须是2的幂
static const size_t kImplicitInitialIndexSize = 32;

/*
    将线程ID映射到隐式生产者的哈希表的初始大小。 请注意，哈希表每次变满一半时都会重新调整大小。
    必须是2的幂，并且为0或至少为1。如果为0，则禁用隐式生产(使用不带显式生产者令牌的入队方法)
*/
static const size_t kInitialImplicitProducerHashSize = 32;

// 控制显式消费者（即具有令牌的消费者）必须消耗的项目数量，然后才会导致所有消费者旋转并转移到下一个内部队列
static const std::uint32_t kExplicitConsumerConsumptionQuotaBeforeRotate = 256;

/*
    可以入队到子队列的最大元素数量（包括在内）。导致超出此限制的入队操作将失败。
    请注意，出于性能原因，此限制在块级别执行，即它会四舍五入到最近的块大小
*/
static const size_t kMaxSubQueueDefaultSize = ConstNumericMax<size_t>::value;

/*
    等待信号量时自旋的次数。建议的值大约在1000到10000之间，除非消费者线程的数量超过了空闲核心的数量
    （在这种情况下，请尝试0到100）。
    仅影响 BlockingConcurrentQueue 的实例
*/
static const int kMaxSemaSpins = 10000;

/*
    是否将动态分配的块回收到内部空闲列表中。如果为 false，则只有预分配的块（由构造函数参数控制）将被回收，
    所有其他块将被释放回堆中。
    请注意，由显式生产者消耗的块仅在队列销毁时被释放（不管此特性如何），而不是在令牌销毁后
*/
static const bool kRecycleAllocatedBlocks = false;



static const size_t kMaxSubqueueSize = 
        (ConstNumericMax<size_t>::value - static_cast<size_t>(kMaxSubQueueDefaultSize) < kBlockSize)
            ? ConstNumericMax<size_t>::value
            : ((static_cast<size_t>(kMaxSubQueueDefaultSize) + (kBlockSize - 1)) 
                / kBlockSize * kBlockSize);


