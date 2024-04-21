#pragma once

#include <stdint.h>

namespace util
{
    #ifdef __x86_64__
        using AtomicWord = uint64_t;
    #elif 
        using AtomicWord = uint32_t;
    #endif 
    /**
    * @brief CAS,将new_value设置到ptr,并返回ptr的值
    * 
    * @param ptr 
    * @param new_value 
    * @return AtomicWord 
    */
    inline AtomicWord NoBarrierCompareAndSwap(volatile AtomicWord* ptr, 
                            AtomicWord new_value)
    {
        AtomicWord old_value;
        do 
        {
            old_value = *ptr;
            // 比较ptr和old_value,如果值相同将new_value更新到ptr并返回true
        } while(!__sync_bool_compare_and_swap(ptr, old_value, new_value));
        return old_value;
    } 

    /**
    * @brief 
    * 
    * @param ptr 
    * @param old_value 
    * @param new_value 
    * @return AtomicWord 
    */
    inline AtomicWord NoBarrierCompareAndSwap(volatile AtomicWord* ptr, 
                                        AtomicWord old_value,
                                        AtomicWord new_value)
    {// 0, 0, 1
        AtomicWord prev_value;
        do 
        {
            // 如果old_value和ptr相等,则将new_value写入ptr
            if (__sync_bool_compare_and_swap(ptr, old_value, new_value))
                return old_value;   // 返回old_value

            prev_value = *ptr;
        } while(prev_value == old_value);
        return prev_value;
    } 


    /**
    * @brief 原子加载
    * 
    * @param ptr 
    * @return AtomicWord 
    */
    inline AtomicWord AcquireLoad(volatile const AtomicWord* ptr)
    {
        AtomicWord value = *ptr;
        __sync_synchronize();
        return value;
    }


    inline AtomicWord AcquireCompareAndSwap(volatile AtomicWord* ptr,
                                        AtomicWord old_value,
                                        AtomicWord new_value)
    {
        return NoBarrierCompareAndSwap(ptr, old_value, new_value);
    }

    /**
    * @brief 原子存储
    * 
    * @param ptr 
    * @param value 
    */
    inline void ReleaseStore(volatile AtomicWord* ptr, AtomicWord value)
    {
        __sync_synchronize();
        *ptr = value;
    }
}

