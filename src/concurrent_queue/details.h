#pragma once

#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

struct ProducerToken; 

using index_t = std::size_t;

using ThreadId_t = std::uintptr_t;
static const ThreadId_t kInvalidThreadId  = 0;		// Address can't be nullptr
static const ThreadId_t kInvalidThreadId2 = 1;		// 空指针之外的成员访问通常也是无效的。 另外，它没有对齐。
inline ThreadId_t ThreadId() 
{ 
	static __thread int x; 
	return reinterpret_cast<ThreadId_t>(&x); 
}

template<typename T>
struct ConstNumericMax 
{
	static_assert(std::is_integral<T>::value, "ConstNumericMax 必须是整型");
	static const T value = std::numeric_limits<T>::is_signed
		? (static_cast<T>(1) << (sizeof(T) * CHAR_BIT - 1)) - static_cast<T>(1)
		: static_cast<T>(-1);
};

enum AllocationMode { CAN_ALLOC, CANNOT_ALLOC };

typedef union 
{
	std::max_align_t x;
	long long y;
	void* z;
} MaxAlign_t;

static bool (likely)(bool x) // 用于告诉编译器提示代码中的分支预向,可以提高编译器优化
{ return __builtin_expect((x), true); }

static bool (unlikely)(bool x) 
{ return __builtin_expect((x), false); }



/**
 * @brief 比如两个循环变量(用于环形数据结构中)
 * 		如果a在b的前方,则返回true.比如a=1, b=2
 *		如果a=2, b=1,则返回false
 * @param a 
 * @param b 
 * @return true 
 * @return false 
 */
template <typename T>
static bool CircularLessThan(T a, T b)
{
    static_assert(std::is_integral<T>::value && !std::numeric_limits<T>::is_signed,
        "CircularLessThan 仅能与无符号整型一起使用");
    return static_cast<T>(a - b) > static_cast<T>(static_cast<T>(1)
        << (static_cast<T>(sizeof(T) * CHAR_BIT - 1)));
}


template<typename T> 
struct StaticIsLockFreeNum { enum { value = 0 }; };

template<> 
struct StaticIsLockFreeNum<signed char> 
{ enum { value = ATOMIC_CHAR_LOCK_FREE }; };

template<> 
struct StaticIsLockFreeNum<short> 
{ enum { value = ATOMIC_SHORT_LOCK_FREE }; };

template<> 
struct StaticIsLockFreeNum<int> 
{ enum { value = ATOMIC_INT_LOCK_FREE }; };

template<> 
struct StaticIsLockFreeNum<long> 
{ enum { value = ATOMIC_LONG_LOCK_FREE }; };

template<> 
struct StaticIsLockFreeNum<long long> 
{ enum { value = ATOMIC_LLONG_LOCK_FREE }; };

template<typename T> 
struct StaticIsLockFree : StaticIsLockFreeNum<typename std::make_signed<T>::type> 
{ };

template<> 
struct StaticIsLockFree<bool> 
{ enum { value = ATOMIC_BOOL_LOCK_FREE }; };

template<typename U> 
struct StaticIsLockFree<U*> 
{ enum { value = ATOMIC_POINTER_LOCK_FREE }; };






template <typename >
struct ThreadIdConverter
{
    using ThreadIdNumericSize_t = ThreadId_t;
    using ThreadIdHash_t = ThreadId_t;
    static ThreadIdHash_t Prehash(ThreadId_t const& x)
    { return x; }
};

template <typename T>
struct Identify 
{ using type = T; };





struct ConcurrentQueueProducerTypelessBase
{
    ConcurrentQueueProducerTypelessBase()
        : next_(nullptr), inactive_(false), token_(nullptr) {}

    ConcurrentQueueProducerTypelessBase* next_;
    std::atomic<bool> inactive_;
    ProducerToken* token_;
};

template <bool use32>
struct hash32Or64_
{
    static inline std::uint32_t Hash(std::uint32_t h)
    {
        h ^= h >> 16;
	    h *= 0x85ebca6b;
	    h ^= h >> 13;
	    h *= 0xc2b2ae35;

	    return h ^ (h >> 16);
    };
};

template <>
struct hash32Or64_<true>
{
    static inline std::uint64_t Hash(std::uint64_t h)
    {
        h ^= h >> 33;
	    h *= 0xff51afd7ed558ccd;
	    h ^= h >> 33;
	    h *= 0xc4ceb9fe1a85ec53;

	    return h ^ (h >> 16);
    };
};

template <std::size_t size>
struct Hash32Or64 : public hash32Or64_<(size > 4)> { };


static inline size_t HashThreadId(ThreadId_t id)
{
	static_assert(sizeof(ThreadId_t) <= 8, "最多为 64 位值的平台");
	return static_cast<size_t>(Hash32Or64<sizeof(
		ThreadIdConverter<ThreadId_t>::ThreadIdHash_t)>::Hash(
		ThreadIdConverter<ThreadId_t>::Prehash(id)));
}
 
template <typename U>
static inline char* AlignFor(char* ptr)
{
    const std::size_t alignment = std::alignment_of<U>::value;
	return ptr + (alignment - 
        (reinterpret_cast<std::uintptr_t>(ptr) % alignment)) % alignment;
}

template<typename T>
static inline T CeilToPow2(T x)
{
	static_assert(std::is_integral<T>::value && !std::numeric_limits<T>::is_signed, 
		"CeilToPow2 仅适用于无符号整数类型");
	
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;

	for (std::size_t i = 1; i < sizeof(T); i <<= 1) 
		x |= x >> (i << 3);
        
	++x;
	return x;
}

template<typename T>
static inline T const& NoMove(T const& x)
{
	return x;
}

template<bool Enable>
struct NoMoveIf
{
	template<typename T>
	static inline T const& Eval(T const& x)
	{
		return x;
	}
};

template<>
struct NoMoveIf<false>
{
	template<typename U>
	static inline auto eval(U&& x)
		-> decltype(std::forward<U>(x))
	{
		return std::forward<U>(x);
	}
};

template<typename It>
static inline auto DerefNoexcept(It& it) noexcept -> decltype(*it)
{
	return *it;
}

template <typename T>
static void SwapRelaxed(std::atomic<T>& left, std::atomic<T>& right)
{
    T tmp = std::move(left.load(std::memory_order_relaxed));
    left.load(std::move(right.load(std::memory_order_relaxed), std::memory_order_relaxed));
    right.store(std::move(tmp), std::memory_order_relaxed);
}

template <typename T>
struct IsTriviallyDestructible : std::is_trivially_destructible<T> {};