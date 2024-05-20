#pragma once

#include <climits>
#include <cstddef>
#include <stdlib.h>
#include <type_traits>
#include <cassert>
#include <new>

#include "details.h"

template<typename TAlign>
static void* AlignedMalloc(size_t size)
{
    if (std::alignment_of<TAlign>::value <= std::alignment_of<MaxAlign_t>::value)
        return malloc(size);
    else
    {
        size_t alignment = std::alignment_of<TAlign>::value;
        void* raw = malloc(size + alignment - 1 + sizeof(void*));
        if (!raw)
            return nullptr;
        char* ptr = AlignFor<TAlign>(reinterpret_cast<char*>(raw) + sizeof(void*));
        *(reinterpret_cast<void**>(ptr) - 1) = raw;
        return ptr;
    }
}

template<typename TAlign>
static void AlignedFree(void* ptr)
{
    if (std::alignment_of<TAlign>::value <= std::alignment_of<MaxAlign_t>::value)
        return free(ptr);
    else
        free(ptr ? *(reinterpret_cast<void**>(ptr) - 1) : nullptr);
}

template<typename U>
static U* CreateArray(size_t count)
{
    assert(count > 0);
    U* p = static_cast<U*>(AlignedMalloc<U>(sizeof(U) * count));
    if (p == nullptr) return nullptr;

    for (size_t i = 0; i != count; i++)
        new (p + i) U();

    return p;
}

template<typename U>
static void DestroyArray(U* p, size_t count)
{
    if (p != nullptr)
    {
        assert(count > 0);
        for (size_t i = count; i!= 0; )
            (p + --i)->~U();
    }
    AlignedFree<U>(p);
}

template<typename U>
static U* Create()
{
    void* p = AlignedMalloc<U>(sizeof(U));
    return p != nullptr ? new (p) U : nullptr;
}

template<typename U, typename A1>
static U* Create(A1&& a1)
{
    void* p = AlignedMalloc<U>(sizeof(U));
    return p != nullptr ? new (p) U(std::forward<A1>(a1)) : nullptr;
}

template<typename U>
static void Destroy(U* p)
{
    if (p != nullptr)
        p->~U();
    AlignedFree<U>(p);
}



