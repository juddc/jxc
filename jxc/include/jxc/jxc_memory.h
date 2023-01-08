#pragma once
#include <string.h>
#include <stdint.h>
#include <memory.h>
#include <memory> // std::allocator

#if !defined(_JXC_CORE_H_)
#error jxc_memory.h must be included after jxc_core.h
#endif

#if JXC_DEBUG
#include <assert.h>
#endif

#if !defined(JXC_ENABLE_DEBUG_ALLOCATOR)
#define JXC_ENABLE_DEBUG_ALLOCATOR 0
#endif

#if !defined(JXC_DEBUG_MEM)
#if JXC_DEBUG && JXC_ENABLE_DEBUG_ALLOCATOR
#define JXC_DEBUG_MEM 1
#else
#define JXC_DEBUG_MEM 0
#endif
#endif

#if JXC_DEBUG_MEM
void _jxc_debug_allocator_notify_allocation(void* ptr, size_t num_bytes);
void _jxc_debug_allocator_notify_deallocation(void* ptr, size_t num_bytes);
#else
JXC_FORCEINLINE void _jxc_debug_allocator_notify_allocation(void*, size_t) {}
JXC_FORCEINLINE void _jxc_debug_allocator_notify_deallocation(void*, size_t) {}
#endif

#if !defined(JXC_MEMCPY)
#if defined(_MSC_VER)
// wrapper around memcpy_s for msvc compat
inline void* _jxc_memcpy(void* dest, size_t dest_len, const void* src, size_t src_len)
{
    const auto result = memcpy_s(dest, dest_len, src, src_len);
#if JXC_DEBUG
    assert(result == 0 && "memcpy failed");
#endif
    return dest;
}
#define JXC_MEMCPY(DEST, DEST_LEN, SRC, SRC_LEN) _jxc_memcpy((DEST), (DEST_LEN), (SRC), (SRC_LEN))
#elif JXC_DEBUG
// wrapper around memcpy that just asserts on the dest_len being large enough to fit src
inline void* _jxc_memcpy(void* dest, size_t dest_len, const void* src, size_t src_len)
{
    assert(dest_len >= src_len);
    return memcpy(dest, src, src_len);
}
#define JXC_MEMCPY(DEST, DEST_LEN, SRC, SRC_LEN) _jxc_memcpy((DEST), (DEST_LEN), (SRC), (SRC_LEN))
#else
#define JXC_MEMCPY(DEST, DEST_LEN, SRC, SRC_LEN) memcpy((DEST), (SRC), (SRC_LEN))
#endif
#endif

#if !defined(JXC_STRNCPY)
inline char* _jxc_strncpy(char* dest, size_t dest_size, const char* src, size_t src_size)
{
    if (dest_size > 0)
    {
        size_t idx = 0;
        while (idx < src_size && src[idx] != '\0')
        {
            dest[idx] = src[idx];
            ++idx;
        }
        // zero the remainder of the dest buffer
        if (idx < dest_size)
        {
            memset(&dest[idx], '\0', dest_size - idx);
        }
    }
    return dest;
}
#define JXC_STRNCPY(DEST, DEST_SIZE, SRC, SRC_SIZE) _jxc_strncpy((DEST), (DEST_SIZE), (SRC), (SRC_SIZE))
#endif

#if !defined(JXC_MALLOC)
#if JXC_DEBUG_MEM
inline void* _jxc_malloc(size_t size)
{
    void* result = malloc(size);
    _jxc_debug_allocator_notify_allocation(result, size);
    return result;
}
#define JXC_MALLOC(SIZE) _jxc_malloc(SIZE)
#else
#define JXC_MALLOC(SIZE) malloc(SIZE)
#endif
#endif

#if !defined(JXC_FREE)
#if JXC_DEBUG_MEM
inline void _jxc_free(void* ptr, size_t size)
{
    _jxc_debug_allocator_notify_deallocation(ptr, size);
    free(ptr);
}
#define JXC_FREE(PTR, SIZE) _jxc_free(PTR, SIZE)
#else
#define JXC_FREE(PTR, SIZE) free(PTR)
#endif
#endif

#if !defined(JXC_STRNDUP)
inline char* _jxc_strndup(const char* str, size_t str_size)
{
    char* result = (char*)JXC_MALLOC(str_size + 1);
    JXC_MEMCPY(result, str_size + 1, str, str_size);
    result[str_size] = '\0';
    return result;
}
#define JXC_STRNDUP(STR, SIZE) _jxc_strndup((STR), (SIZE))
#endif

#if !defined(JXC_ALLOCATOR)
#define JXC_ALLOCATOR ::std::allocator
#endif

#if JXC_DEBUG_MEM

#if defined(JXC_ALLOCATOR)
#undef JXC_ALLOCATOR
#endif

#define JXC_ALLOCATOR ::_jxc_debug_allocator

void _jxc_debug_allocator_print_all_allocations(const char* prefix);
int64_t _jxc_debug_num_allocator_allocations();
int64_t _jxc_debug_num_global_new_free_allocations();
inline int64_t _jxc_debug_num_allocations() { return _jxc_debug_num_allocator_allocations() + _jxc_debug_num_global_new_free_allocations(); }

template<typename T>
struct _jxc_debug_allocator : public std::allocator<T>
{
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = size_t;

    template<typename U>
    struct rebind
    {
        using other = _jxc_debug_allocator<U>;
    };

    _jxc_debug_allocator()
    {
    }

    template<typename U>
    _jxc_debug_allocator(const _jxc_debug_allocator<U>& u)
        : std::allocator<T>(u)
    {
    }

    pointer allocate(size_type count)
    {
        pointer result = std::allocator<T>::allocate(count);
        _jxc_debug_allocator_notify_allocation((void*)result, sizeof(T) * count);
        return result;
    }

    void deallocate(pointer ptr, size_type count)
    {
        _jxc_debug_allocator_notify_deallocation((void*)ptr, sizeof(T) * count);
        std::allocator<T>().deallocate(ptr, count);
    }
};

//void* operator new(std::size_t size);
//void operator delete(void* mem);

namespace jxc::detail
{
struct DebugAllocationCheck
{
    const char* label;
    bool assert_on_fail;
    bool* out_has_leaks;
    int64_t start_allocator_count;
    int64_t start_global_count;

    DebugAllocationCheck(const char* label = "Debug Allocation Check", bool assert_on_fail = false, bool* out_has_leaks = nullptr);
    ~DebugAllocationCheck();
    int64_t num_allocator_allocations() const;
    bool no_leaks() const;
};
} // namespace jxc::detail

#else

JXC_FORCEINLINE void _jxc_debug_allocator_print_all_allocations(const char*) {}
JXC_FORCEINLINE int64_t _jxc_debug_num_allocator_allocations() { return 0; }
JXC_FORCEINLINE int64_t _jxc_debug_num_global_new_free_allocations() { return 0; }
JXC_FORCEINLINE int64_t _jxc_debug_num_allocations() { return 0; }

#endif
