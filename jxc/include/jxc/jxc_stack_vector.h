#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include "jxc/jxc_core.h"
#include "jxc/jxc_type_traits.h"
#include "jxc/jxc_memory.h"


JXC_BEGIN_NAMESPACE(jxc)
JXC_BEGIN_NAMESPACE(detail)


/// Templated pointer type for arrays that stores a pointer to the first element in the array as well as the length of that array.
/// NB. This is NOT a managed pointer, just a pointer that keeps metadata about itself.
template<typename T>
struct FatPointer
{
    T* ptr = nullptr;
    size_t size = 0;

    static constexpr size_t npos = std::numeric_limits<size_t>::max();

    FatPointer() = default;
    FatPointer(nullptr_t) : ptr(nullptr), size(0) {}
    FatPointer(T* ptr, size_t size) : ptr(ptr), size(size) {}

    /// allow implicitly constructing from array types
    template<JXC_CONCEPT(traits::HasDataAndSize<T>) RhsT>
    explicit FatPointer(const RhsT& vec) : ptr(const_cast<T*>(vec.data())), size(vec.size()) {}

    inline T& operator[](size_t idx) { JXC_ASSERT(ptr != nullptr); JXC_INDEX_ASSERT(idx, size); return ptr[idx]; }
    inline const T& operator[](size_t idx) const { JXC_ASSERT(ptr != nullptr); JXC_INDEX_ASSERT(idx, size); return ptr[idx]; }

    inline T* item_ptr_unchecked(size_t idx) { return &ptr[idx]; }
    inline const T* item_ptr_unchecked(size_t idx) const { return &ptr[idx]; }

    inline T* item_ptr(size_t idx) { JXC_ASSERT(ptr != nullptr); JXC_INDEX_ASSERT(idx, size); return &ptr[idx]; }
    inline const T* item_ptr(size_t idx) const { JXC_ASSERT(ptr != nullptr); JXC_INDEX_ASSERT(idx, size); return &ptr[idx]; }

    /// Returns a pointer at the beginning of the data. Useful for iterators. 
    inline T* begin_ptr() { JXC_ASSERT(ptr != nullptr); return ptr; }
    inline const T* begin_ptr() const { JXC_ASSERT(ptr != nullptr); return ptr; }

    /// Returns a pointer that is one _past_ the end of the data. Useful for iterators. 
    inline T* end_ptr() { JXC_ASSERT(ptr != nullptr); return &ptr[size]; }
    inline const T* end_ptr() const { JXC_ASSERT(ptr != nullptr); return &ptr[size]; }

    inline explicit operator bool() const { return ptr != nullptr && size > 0; }

    inline bool operator==(FatPointer<T> rhs) const { return ptr == rhs.ptr && size == rhs.size; }
    inline bool operator==(T* rhs) const { return ptr == rhs; }
    inline bool operator==(nullptr_t) const { return ptr == nullptr; }

    inline bool operator!=(FatPointer<T> rhs) const { return !operator==(rhs); }
    inline bool operator!=(T* rhs) const { return ptr != rhs; }
    inline bool operator!=(nullptr_t) const { return ptr != nullptr; }

    template<typename DestT>
    inline DestT* reinterpret_ptr()
    {
        return reinterpret_cast<DestT*>(ptr);
    }

    template<typename DestT>
    inline const DestT* reinterpret_ptr() const
    {
        return reinterpret_cast<const DestT*>(ptr);
    }

    /// Compares all bytes with another pointer and returns true if they are the same.
    bool mem_equals(FatPointer<T> rhs) const
    {
        return size == rhs.size && memcmp(ptr, rhs.ptr, sizeof(T) * size) == 0;
    }

    /// Compares all bytes with another pointer and returns true if they are the same.
    /// Uses strcmp or wcscmp if the types are appropriate, otherwise memcmp.
    bool string_equals(FatPointer<T> rhs) const
    {
        if constexpr (std::is_same_v<T, char>)
        {
            return strcmp(ptr, rhs.ptr) == 0;
        }
        else if constexpr (std::is_same_v<T, wchar_t>)
        {
            return wcscmp(ptr, rhs.ptr) == 0;
        }
        else
        {
            return size == rhs.size && memcmp(ptr, rhs.ptr, size) == 0;
        }
    }

    /// Returns an unowned slice of the data in this pointer
    FatPointer<T> slice(size_t start_idx, size_t count = npos) const
    {
        if (start_idx >= size)
        {
            return nullptr;
        }

        if (count == npos || start_idx + count > size)
        {
            count = size - start_idx;
        }

        T* new_ptr = ptr + start_idx;
        JXC_ASSERT(*reinterpret_cast<intptr_t*>(&new_ptr[count - 1]) == *reinterpret_cast<intptr_t*>(ptr + start_idx + count - 1));
        return FatPointer<T>(new_ptr, count);
    }

private:
    static inline void memcpy_single_internal(T* dst, const T* src)
    {
        JXC_DEBUG_ASSERT(dst != nullptr);
        JXC_DEBUG_ASSERT(src != nullptr);
        JXC_MEMCPY((void*)dst, sizeof(T), (const void*)src, sizeof(T));
    }

    static inline void memcpy_multi_internal(T* dst, size_t dst_count, const T* src, size_t src_count)
    {
        JXC_DEBUG_ASSERT(dst != nullptr);
        JXC_DEBUG_ASSERT(src != nullptr);
        JXC_MEMCPY((void*)dst, sizeof(T) * dst_count, (const void*)src, sizeof(T) * src_count);
    }

    static inline void memzero_internal(T* dst, size_t count)
    {
        memset((void*)dst, 0, sizeof(T) * count);
    }

    static inline void fillzero_internal(T* dst, size_t count)
    {
        uint8_t* dst_ptr = (uint8_t*)dst;
        for (size_t i = 0; i < sizeof(T) * count; i++)
        {
            dst_ptr[i] = 0;
        }
    }

    static inline void memmove_internal(T* dst, size_t dst_size, const T* src, size_t src_size)
    {
        JXC_MEMMOVE((void*)dst, dst_size, (void*)src, src_size);
    }

public:
    inline void memzero()
    {
        memzero_internal(ptr, size);
    }

    inline void fillzero()
    {
        fillzero_internal(ptr, size);
    }

    inline void memmove_from(FatPointer<T> other)
    {
        memmove_internal(ptr, size, other.ptr, other.size);
    }

    /// Copies all memory from another pointer into this pointer.
    /// Requires that our size is at least large enough to contain all of that data.
    inline void copy_from(FatPointer<T> other)
    {
        JXC_ASSERT(size >= other.size);
        memcpy_multi_internal(ptr, size, other.ptr, other.size);

        // Zero out all memory past the point where we stopped copying from other.
        // We don't want any junk memory past the end of the data.
        if (size > other.size)
        {
            memzero_internal(item_ptr_unchecked(other.size), size - other.size);
        }
    }

    inline void zero_element(size_t index)
    {
        JXC_INDEX_ASSERT(index, size);
        fillzero_internal(item_ptr_unchecked(index), 1);
    }

    /// assuming this pointer is a dynamic array (has a size > 0), copies one element in that dynamic array to another slot in the same array.
    inline void copy_element(size_t dst_index, size_t src_index)
    {
        JXC_INDEX_ASSERT(dst_index, size);
        JXC_INDEX_ASSERT(src_index, size);
        JXC_ASSERT(dst_index != src_index);
        memcpy_single_internal(item_ptr_unchecked(dst_index), item_ptr_unchecked(src_index));
    }

    /// copies one element from this pointer to an index in another pointer
    inline void copy_element(FatPointer<T> dst_ptr, size_t dst_index, size_t src_index) const
    {
        JXC_ASSERT(ptr != nullptr);
        JXC_ASSERT(dst_ptr.ptr != nullptr);
        JXC_INDEX_ASSERT(dst_index, dst_ptr.size);
        JXC_INDEX_ASSERT(src_index, size);
        memcpy_single_internal(dst_ptr.item_ptr_unchecked(dst_index), item_ptr_unchecked(src_index));
    }

    void swap_element(size_t index_a, size_t index_b)
    {
        JXC_INDEX_ASSERT(index_a, size);
        JXC_INDEX_ASSERT(index_b, size);
        JXC_ASSERT(index_a != index_b);
        uint8_t tmp[sizeof(T)] = { 0 };
        FatPointer<T> tmp_ptr = FatPointer<T>(reinterpret_cast<T*>(&tmp[0]), 1);
        copy_element(tmp_ptr, 0, index_a);          // tmp = a
        copy_element(index_a, index_b);             // data[a] = data[b]
        tmp_ptr.copy_element(*this, index_b, 0);    // data[b] = tmp
    }

    void swap_element(FatPointer<T> rhs, size_t rhs_index, size_t index)
    {
        JXC_ASSERT(rhs.ptr != nullptr);
        JXC_INDEX_ASSERT(rhs_index, rhs.size);
        JXC_INDEX_ASSERT(index, size);
        uint8_t tmp[sizeof(T)] = { 0 };
        auto tmp_ptr = FatPointer<T>((T*)&tmp[0], 1);
        rhs.copy_element(tmp_ptr, 0, rhs_index);    // tmp = rhs_value
        copy_element(rhs, rhs_index, index);        // rhs_value = our_value
        tmp_ptr.copy_element(*this, index, 0);      // our_value = tmp
    }
};


/// Iterate over a range with a custom getter function
template<typename ContainerType, typename ValueType, typename IdxType, bool IsConst = false>
class CustomIteratorBase
{
public:
    using value_type = ValueType;
    using difference_type = std::conditional_t<std::is_signed_v<IdxType>, IdxType, int64_t>;
    using pointer = std::conditional_t<IsConst, const ValueType*, ValueType*>;
    using reference = std::conditional_t<IsConst, const ValueType&, ValueType&>;
    using iterator_category = std::forward_iterator_tag;

    using container_reference = std::conditional_t<IsConst, const ContainerType&, ContainerType&>;
    using index_type = IdxType;

    using getter_func = JXC_DEFINE_FUNCTION_POINTER(reference, container_reference, index_type);
    using increment_func = JXC_DEFINE_FUNCTION_POINTER(void, index_type&);

private:
    container_reference container;
    index_type idx;
    getter_func get = nullptr;
    increment_func increment = nullptr;

public:
    CustomIteratorBase(container_reference container, index_type idx, getter_func get, increment_func increment)
        : container(container)
        , idx(idx)
        , get(get)
        , increment(increment)
    {
    }

    CustomIteratorBase(container_reference container, index_type idx)
        : container(container)
        , idx(idx)
    {
    }

    static inline CustomIteratorBase make_end_iterator(container_reference container, index_type idx)
    {
        return CustomIteratorBase(container, idx);
    }

    inline reference operator*() const { JXC_DEBUG_ASSERT(get != nullptr); return get(container, idx); }
    inline CustomIteratorBase& operator++() { JXC_DEBUG_ASSERT(increment != nullptr); increment(idx); return *this; }
    inline bool operator==(CustomIteratorBase& rhs) const { return idx == rhs.idx; }
    //inline bool operator!=(CustomIteratorBase& rhs) const { return idx != rhs.idx; }
};


template<typename T>
struct StackVectorAllocator
{
    static inline FatPointer<T> alloc(size_t num_elements)
    {
        FatPointer<T> result = { static_cast<T*>(::malloc(sizeof(T) * num_elements)), num_elements };
        result.memzero();
        return result;
    }

    static inline void free(FatPointer<T> ptr)
    {
        if (ptr.ptr != nullptr)
        {
            ::free(ptr.ptr);
        }
    }
};


/// Similar to a std::vector, but can store a small amount of items on the stack, and will not allocate heap memory
/// unless the capacity needs to go beyond that size.
template<typename T, uint16_t BufferSize = 16, typename AllocatorType = StackVectorAllocator<T>>
class StackVector
{
public:
    static constexpr size_t buffer_size = static_cast<size_t>(BufferSize);

    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = PointerIterator<T>;
    using const_iterator = ConstPointerIterator<T>;

    static constexpr size_type invalid_index = std::numeric_limits<size_type>::max();

private:
    using allocator = AllocatorType;

    enum DataOriginType : uint8_t
    {
        on_stack = 0,
        on_heap = 1,
    };

    static constexpr bool run_constructors_and_destructors = !std::is_trivial_v<T>;

    static_assert(!run_constructors_and_destructors || std::is_default_constructible_v<T>,
        "types stored in a StackVector must be default-constructible");

    /// extra number of items to allocate past the required amount when doing heap allocations
    static constexpr size_t heap_extra_slots = 16;

    /// data is stored as a union - either a static array or a pointer and size
    union
    {
        T stack_data[buffer_size];

        struct
        {
            T* heap_data;
            size_t heap_capacity;
        };
    };

    /// data_origin determines which union values we should be using
    DataOriginType data_origin = on_stack;

    /// number of items currently in use (may be <= capacity)
    size_t num_elements = 0;

    /// zeroes out stack_data. mainly for initializing things properly in constructors.
    inline void memzero_stack_data()
    {
        auto stack_data = FatPointer<T>(&const_cast<StackVector*>(this)->stack_data[0], buffer_size);
        stack_data.memzero();
    }

    /// allocates a blob of memory on the heap, intelligently bumping up extra capacity for larger sizes.
    FatPointer<T> heapalloc(size_t new_capacity)
    {
        size_t extra = heap_extra_slots;
        if (new_capacity > 2048)
        {
            extra *= 32;
        }
        else if (new_capacity > 1024)
        {
            extra *= 16;
        }
        else if (new_capacity > 512)
        {
            extra *= 8;
        }
        else if (new_capacity > 256)
        {
            extra *= 4;
        }
        else if (new_capacity > 128)
        {
            extra *= 2;
        }

        return allocator::alloc(new_capacity + extra);
    }

    /// returns all allocated data currently in use
    inline FatPointer<T> get_allocated_data() const
    {
        switch (data_origin)
        {
        case on_stack:
            return FatPointer<T>(&const_cast<StackVector*>(this)->stack_data[0], buffer_size);
        case on_heap:
            return FatPointer<T>(heap_data, heap_capacity);
        default:
            break;
        }
        return FatPointer<T>();
    }

    /// gets the slice of allocated memory we're using for actual data values
    inline FatPointer<T> get_used_data() const
    {
        auto data = get_allocated_data();
        return (num_elements == data.size) ? data : data.slice(0, num_elements);
    }

    /// Sets heap data as the current allocated data.
    /// Does no copying of any kind.
    void set_heap_data(FatPointer<T> new_data)
    {
        JXC_ASSERT(new_data.ptr != nullptr);
        JXC_ASSERT(new_data.size > 0);

        switch (data_origin)
        {
        case on_stack:
            // zero out stack memory before switching from stack to heap data
            get_allocated_data().memzero();
            break;
        case on_heap:
            if (heap_data != nullptr)
            {
                // clear out existing memory
                allocator::free(get_allocated_data());
                heap_data = nullptr;
                heap_capacity = 0;
            }
            break;
        default:
            break;
        }

        data_origin = on_heap;
        heap_data = new_data.ptr;
        heap_capacity = new_data.size;
    }

    /// replaces a stack allocation with a heap allocation
    void realloc_from_stack_to_heap(size_t new_capacity)
    {
        JXC_ASSERT(data_origin == on_stack);
        JXC_ASSERT(new_capacity >= num_elements);

        auto new_data = heapalloc(new_capacity);
        new_data.copy_from(get_used_data());

        set_heap_data(new_data);
    }

    /// takes an existing heap allocation and replaces it with a larger one
    void enlarge_heap_memory(size_t new_capacity)
    {
        JXC_ASSERT(data_origin == on_heap);
        JXC_ASSERT(heap_data != nullptr);
        JXC_ASSERT(heap_capacity > 0);
        JXC_ASSERT(new_capacity >= num_elements);

        auto new_data = heapalloc(new_capacity);
        new_data.copy_from(get_used_data());

        set_heap_data(new_data);
    }

    /// Shrinks allocated capacity.
    /// If we're currently using a heap allocation and new_capacity is less than the max stack capacity, switches back to stack allocation.
    void shrink_capacity(size_t new_capacity)
    {
        if (data_origin == on_stack || (data_origin == on_heap && new_capacity >= heap_capacity))
        {
            return;
        }

        JXC_ASSERTF(data_origin == on_heap, "Can only shrink heap data");

        if (new_capacity < buffer_size)
        {
            // switch to stack
            auto existing_mem = FatPointer<T>(heap_data, heap_capacity);
            auto existing_data = FatPointer<T>(heap_data, num_elements);

            // switch to stack memory and zero it all out
            data_origin = on_stack;
            auto stackmem = get_allocated_data();
            stackmem.memzero();

            const size_t new_size = (new_capacity < num_elements) ? new_capacity : num_elements;

            // make sure stack memory is cleared before copying data into it
            stackmem.copy_from(existing_data.slice(0, new_size));

            // run destructors on deleted items
            if constexpr (run_constructors_and_destructors)
            {
                auto deleted_items = existing_data.slice(new_size);
                for (size_t i = 0; i < deleted_items.size; i++)
                {
                    deleted_items[i].~T();
                }
            }

            num_elements = new_size;

            allocator::free(existing_mem);
        }
        else
        {
            auto new_data = heapalloc(new_capacity);

            if (num_elements > 0)
            {
                new_data.copy_from(get_used_data());
            }

            allocator::free(get_allocated_data());

            set_heap_data(new_data);
        }
    }

    /// Initializes the vector from a container size and getter function in the form `[](size_t idx) -> T` or `[](size_t idx) -> T&&`.
    /// Will call getter_fn once for each index.
    /// Intended for use from constructors
    template<typename Lambda>
    void init_from_lambda(size_t container_size, Lambda&& getter_fn)
    {
        JXC_ASSERTF(num_elements == 0, "init_from_lambda requires at least one value");

        static_assert(std::is_same_v<traits::BaseType<traits::FunctionReturnType<Lambda>>, T>, "getter_fn must return values of type T");
        constexpr bool move_items = std::is_same_v<traits::FunctionReturnType<Lambda>, T&&>;

        if (container_size < buffer_size)
        {
            data_origin = on_stack;
            num_elements = container_size;

            auto stackmem = get_allocated_data();
            stackmem.memzero();

            for (size_t i = 0; i < container_size; i++)
            {
                if constexpr (move_items)
                {
                    stackmem[i] = std::move(getter_fn(i));
                }
                else
                {
                    stackmem[i] = getter_fn(i);
                }
            }
        }
        else
        {
            data_origin = on_heap;
            num_elements = container_size;

            auto new_data = heapalloc(container_size);

            for (size_t i = 0; i < container_size; i++)
            {
                if constexpr (move_items)
                {
                    new_data[i] = std::move(getter_fn(i));
                }
                else
                {
                    new_data[i] = getter_fn(i);
                }
            }

            set_heap_data(new_data);
        }
    }

    template<typename IterableContainer>
    void init_from_iterable(const IterableContainer& container)
    {
        const size_t container_size = container.size();
        if (container_size == 0)
        {
            return;
        }

        auto iter = container.begin();
        init_from_lambda(container_size, [&iter](size_t idx) -> const T&
            {
                const T& result = *iter;
                ++iter;
                return result;
            });
        JXC_ASSERTF(iter == container.end(), "failed to iterate through all items");
    }

    // intended for use by constructors
    void copy_vector_from(const StackVector& rhs)
    {
        JXC_ASSERTF(num_elements == 0, "copying into non-empty StackVector");

        if (rhs.size() == 0)
        {
            return;
        }

        num_elements = rhs.num_elements;
        data_origin = (rhs.num_elements <= buffer_size) ? on_stack : on_heap;
        if (data_origin == on_heap)
        {
            set_heap_data(heapalloc(rhs.num_elements));
        }

        auto data = get_allocated_data();
        auto rhs_data = rhs.get_used_data();

        if constexpr (run_constructors_and_destructors)
        {
            // need to use copy constructors to copy elements
            for (size_t i = 0; i < rhs.num_elements; i++)
            {
                data[i] = rhs_data[i];
            }
        }
        else
        {
            // don't care about constructors and destructors, we can just do a direct copy for speed
            data.copy_from(rhs_data);
        }
    }

    // intended for use by constructors
    void move_vector_from(StackVector&& rhs)
    {
        JXC_ASSERTF(num_elements == 0, "moving into non-empty StackVector");

        if (rhs.size() == 0)
        {
            return;
        }

        data_origin = rhs.data_origin;

        if (rhs.data_origin == on_heap)
        {
            set_heap_data(rhs.get_allocated_data());
            num_elements = rhs.num_elements;
            rhs.heap_data = nullptr;
            rhs.heap_capacity = 0;
            rhs.num_elements = 0;
            rhs.data_origin = on_stack;
        }
        else
        {
            JXC_DEBUG_ASSERT(rhs.data_origin == on_stack);

            auto data = get_allocated_data();
            auto rhs_data = rhs.get_used_data();

            // need to move all items individually
            if constexpr (run_constructors_and_destructors)
            {
                // need to use move constructors to move elements
                for (size_t i = 0; i < rhs.num_elements; i++)
                {
                    new (data.item_ptr_unchecked(i)) T(std::move(rhs_data[i]));
                }
            }
            else
            {
                // don't care about constructors and destructors, so we can just do a direct copy for speed
                data.copy_from(rhs_data);
            }

            num_elements = rhs.num_elements;
            rhs.num_elements = 0;
        }
    }

    // intended for use by constructors
    void copy_from_raw_data(FatPointer<T> raw_data)
    {
        resize(raw_data.size);
        if constexpr (run_constructors_and_destructors)
        {
            FatPointer<T> data = get_allocated_data();
            for (size_t i = 0; i < raw_data.size; i++)
            {
                data[i] = raw_data[i];
            }
        }
        else
        {
            // fast path - can use memcpy for this
            get_allocated_data().copy_from(raw_data);
        }
    }

public:
    StackVector()
        : data_origin(on_stack)
    {
        memzero_stack_data();
    }

    explicit StackVector(size_type initial_size)
        : data_origin(on_stack)
    {
        memzero_stack_data();
        resize(initial_size);
    }

    ~StackVector()
    {
        if constexpr (run_constructors_and_destructors)
        {
            FatPointer<T> data = get_used_data();
            for (size_t i = 0; i < num_elements; i++)
            {
                data[i].~T();
            }
        }

        if (data_origin == on_heap)
        {
            allocator::free(get_allocated_data());
            heap_data = nullptr;
            heap_capacity = 0;
        }
    }

    StackVector(const StackVector& rhs)
    {
        memzero_stack_data();
        copy_vector_from(rhs);
    }

    StackVector(FatPointer<T> existing_data_ptr)
    {
        memzero_stack_data();
        copy_from_raw_data(existing_data_ptr);
    }

    template<JXC_CONCEPT(traits::HasDataAndSize<T>) RhsT>
    StackVector(const RhsT& rhs)
        : StackVector(FatPointer<T>(rhs))
    {
    }

    StackVector(std::initializer_list<T> elems)
    {
        memzero_stack_data();
        init_from_iterable(elems);
        std::vector<T> v;
    }

    template<typename IteratorT>
    StackVector(IteratorT first, IteratorT last)
    {
        memzero_stack_data();
        IteratorT iter = first;
        while (iter != last)
        {
            push_back(*iter);
            ++iter;
        }
    }

    StackVector(StackVector&& rhs) noexcept
    {
        memzero_stack_data();
        move_vector_from(std::move(rhs));
    }

    StackVector& operator=(const StackVector& rhs)
    {
        if (this != &rhs)
        {
            clear();
            copy_vector_from(rhs);
        }
        return *this;
    }

    StackVector& operator=(StackVector&& rhs) noexcept
    {
        if (this != &rhs)
        {
            clear(true);
            move_vector_from(std::move(rhs));
        }
        return *this;
    }

#if JXC_CPP20

    template<typename RhsT, uint16_t RhsBufferSize, typename RhsAllocatorType>
    bool operator==(const StackVector<RhsT, RhsBufferSize, RhsAllocatorType>& rhs) const
    {
        if (size() != rhs.size())
        {
            return false;
        }

        FatPointer<T> lhs_data = get_used_data();
        FatPointer<RhsT> rhs_data = rhs.data();

        // if we can use `operator==` to compare the values, use that, otherwise fall back on memcmp.
        if constexpr (std::equality_comparable_with<T, RhsT>)
        {
            for (size_t i = 0; i < size(); i++)
            {
                const bool values_equal = lhs_data[i] == rhs_data[i];
                if (!values_equal)
                {
                    return false;
                }
            }
        }
        else if constexpr (std::is_same_v<T, RhsT>)
        {
            return lhs_data.mem_equals(rhs_data);
        }
        else
        {
            // can't compare these value types...
            static_assert(std::equality_comparable_with<T, RhsT> || std::is_same_v<T, RhsT>,
                "Can't compare StackVectors - types are not comparable (do they have operator==?)");
            return false;
        }

        return true;
    }

    template<typename RhsT, uint16_t RhsBufferSize, typename RhsAllocatorType>
    inline bool operator!=(const StackVector<RhsT, RhsBufferSize, RhsAllocatorType>& rhs) const
    {
        return !operator==<RhsT, RhsBufferSize, RhsAllocatorType>(rhs);
    }

#endif // JXC_CPP20

    inline size_type size() const
    {
        return num_elements;
    }

    inline size_type capacity() const
    {
        return (data_origin == on_stack) ? buffer_size : heap_capacity;
    }

    void swap(StackVector& rhs)
    {
        if (this == &rhs)
        {
            return;
        }

        if (data_origin == on_heap && rhs.data_origin == on_heap)
        {
            // best case - both vectors are using heap data, so we can just swap pointers
            std::swap(heap_data, rhs.heap_data);
            std::swap(heap_capacity, rhs.heap_capacity);
            std::swap(num_elements, rhs.num_elements);
        }
        else if (data_origin == on_stack && rhs.data_origin == on_stack)
        {
            // both vectors are using stack data, so we'll have to do element-wise swaps
            FatPointer<T> lhs_data = get_allocated_data();
            FatPointer<T> rhs_data = rhs.get_allocated_data();
            const size_t num_swaps = std::max(num_elements, rhs.num_elements);
            JXC_ASSERT(lhs_data.size == rhs_data.size);
            JXC_ASSERT(num_swaps <= num_elements);

            for (size_t i = 0; i < num_swaps; i++)
            {
                lhs_data.swap_element(rhs_data, i, i);
            }

            std::swap(num_elements, rhs.num_elements);
        }
        else
        {
            // worst case - one container is using stack data and the other is using heap data
            T tmp[buffer_size];
            FatPointer<T> tmp_data = FatPointer<T>(&tmp[0], buffer_size);
            size_t tmp_data_num_elements = 0;

            auto swap_stack_and_heap = [&tmp_data, &tmp_data_num_elements](StackVector& stack_vec, StackVector& heap_vec)
            {
                JXC_DEBUG_ASSERT(stack_vec.data_origin == on_stack);
                JXC_DEBUG_ASSERT(heap_vec.data_origin == on_heap);

                // tmp = stack_vec
                tmp_data.copy_from(stack_vec.get_used_data());
                tmp_data_num_elements = stack_vec.num_elements;

                // stack_vec = heap_vec
                stack_vec.set_heap_data(heap_vec.get_allocated_data());
                stack_vec.num_elements = heap_vec.num_elements;

                // heap_vec = tmp
                heap_vec.heap_data = nullptr;
                heap_vec.heap_capacity = 0;
                heap_vec.data_origin = on_stack;
                heap_vec.get_allocated_data().copy_from(tmp_data.slice(0, tmp_data_num_elements));
                heap_vec.num_elements = tmp_data_num_elements;
            };

            if (data_origin == on_stack)
            {
                swap_stack_and_heap(*this, rhs);
            }
            else
            {
                swap_stack_and_heap(rhs, *this);
            }
        }
    }

    // allow using std::swap on StackVectors
    friend inline void swap(StackVector& a, StackVector& b)
    {
        a.swap(b);
    }

    inline value_type& operator[](size_type idx)
    {
        JXC_INDEX_ASSERT(idx, num_elements);
        return get_allocated_data()[idx];
    }

    inline const value_type& operator[](size_type idx) const
    {
        JXC_INDEX_ASSERT(idx, num_elements);
        return get_allocated_data()[idx];
    }

    iterator begin()
    {
        return iterator{ get_used_data().begin_ptr() };
    }

    iterator end()
    {
        return iterator{ get_used_data().end_ptr() };
    }

    const_iterator begin() const
    {
        return const_iterator{ get_used_data().begin_ptr() };
    }

    const_iterator end() const
    {
        return const_iterator{ get_used_data().end_ptr() };
    }

    // using iterator = CustomIteratorBase<StackVector, value_type, size_type, false>;
    // using const_iterator = CustomIteratorBase<StackVector, value_type, size_type, true>;

    // iterator begin()
    // {
    //     return iterator(*this, 0,
    //         [](typename iterator::container_reference container, size_type idx) -> typename iterator::reference { return container[idx]; },
    //         [](size_type& idx) { ++idx; });
    // }

    // iterator end()
    // {
    //     return iterator::make_end_iterator(*this, num_elements);
    // }

    // const_iterator begin() const
    // {
    //     return const_iterator(*this, 0,
    //         [](typename const_iterator::container_reference container, size_type idx) -> typename const_iterator::reference { return container[idx]; },
    //         [](size_type& idx) { ++idx; });
    // }

    // const_iterator end() const
    // {
    //     return const_iterator::make_end_iterator(*this, num_elements);
    // }

    void reserve(size_type new_capacity)
    {
        if (new_capacity <= capacity() || (data_origin == on_stack && new_capacity <= buffer_size))
        {
            return;
        }

        if (data_origin == on_stack)
        {
            realloc_from_stack_to_heap(new_capacity);
        }
        else
        {
            enlarge_heap_memory(new_capacity);
        }
    }

    void resize(size_type new_size, const T* default_value = nullptr, bool allow_shrinking = true)
    {
        const size_t orig_size = num_elements;

        if constexpr (run_constructors_and_destructors)
        {
            if (new_size < orig_size)
            {
                // run destructors on removed items
                auto data = get_used_data();
                for (size_t i = new_size; i < orig_size; i++)
                {
                    data[i].~T();
                }
            }
        }

        if (new_size > capacity())
        {
            reserve(new_size);
        }
        else if (allow_shrinking && data_origin == on_heap && new_size <= buffer_size)
        {
            shrink_capacity(new_size);
            JXC_ASSERT(data_origin == on_stack);
        }

        num_elements = new_size;

        if constexpr (run_constructors_and_destructors)
        {
            if (new_size > orig_size)
            {
                auto data = get_used_data();

                if constexpr (std::is_default_constructible_v<T>)
                {
                    if (default_value == nullptr)
                    {
                        // default-construct extra items
                        for (size_t i = orig_size; i < new_size; i++)
                        {
                            new (data.item_ptr_unchecked(i)) T();
                        }
                    }
                    else
                    {
                        for (size_t i = orig_size; i < new_size; i++)
                        {
                            // copy-construct from default_value
                            new (data.item_ptr_unchecked(i)) T(*default_value);
                        }
                    }
                }
                else
                {
                    JXC_ASSERTF(default_value != nullptr, "Type is not default-constructible, so a default value is required when resizing");
                    for (size_t i = orig_size; i < new_size; i++)
                    {
                        // copy-construct from default_value
                        new (data.item_ptr_unchecked(i)) T(*default_value);
                    }
                }
            }
        }
        else
        {
            // constructors are not required. if we have a default value, just copy that into the new slots
            if (default_value != nullptr)
            {
                auto data = get_used_data();
                for (size_t i = orig_size; i < new_size; i++)
                {
                    *data.item_ptr_unchecked(i) = *default_value;
                }
            }
        }
    }

    // allow taking a reference to a default value - much easier to use
    inline void resize(size_type new_size, const T& default_value, bool allow_shrinking = true)
    {
        resize(new_size, &default_value, allow_shrinking);
    }

    void clear(bool allow_shrinking = false)
    {
        if (num_elements == 0)
        {
            return;
        }

        if constexpr (run_constructors_and_destructors)
        {
            if (auto data = get_used_data())
            {
                for (size_t i = 0; i < data.size; i++)
                {
                    data[i].~T();
                }
            }
        }

        if (data_origin == on_heap)
        {
            if (allow_shrinking)
            {
                JXC_ASSERT(heap_data != nullptr);
                allocator::free(get_allocated_data());
                data_origin = on_stack;
            }
            else
            {
                // zero out heap memory for sanity
                get_allocated_data().memzero();
            }
        }

        if (data_origin == on_stack)
        {
            // zero all stack-allocated memory
            get_allocated_data().memzero();
        }

        num_elements = 0;
    }

    /// returns true if this container owns any heap data (false if all data is on the stack)
    inline bool has_heap_allocation() const
    {
        return data_origin == on_heap && heap_data != nullptr;
    }

    inline bool is_valid_index(size_type idx) const
    {
        return _jxc_index_is_valid(idx, num_elements);
    }

#if JXC_CPP20

    /// Finds the index of the specified value, starting the search from the beginning of the array
    size_type find_index(const T& value) const
    {
        if constexpr (std::equality_comparable<T>)
        {
            auto data = get_used_data();
            for (size_t i = 0; i < data.size; i++)
            {
                if (value == data[i])
                {
                    return i;
                }
            }
            return invalid_index;
        }
        return invalid_index;
    }

    /// Finds the index of the specified value, starting the search from the end of the array
    size_type find_index_from_end(const T& value) const
    {
        if constexpr (std::equality_comparable<T>)
        {
            auto data = get_used_data();
            if (data.size == 0)
            {
                return invalid_index;
            }

            for (int64_t i = (int64_t)data.size - 1; i >= 0; i--)
            {
                if (value == data[i])
                {
                    return (size_type)i;
                }
            }
            return invalid_index;
        }
        return invalid_index;
    }

    inline bool contains_value(const T& value) const
    {
        return find_index(value) != invalid_index;
    }

    template<typename Lambda>
    bool contains_by_predicate(Lambda&& predicate) const
    {
        if constexpr (std::equality_comparable<T>)
        {
            auto data = get_used_data();
            for (size_t i = 0; i < data.size; i++)
            {
                if (predicate(data[i]))
                {
                    return true;
                }
            }
            return false;
        }
        return false;
    }

#endif // JXC_CPP20

    template<typename Lambda>
    T* find_by_predicate(Lambda&& predicate)
    {
        FatPointer<T> data = get_used_data();
        for (size_t i = 0; i < num_elements; i++)
        {
            const T& item = data[i];
            if (predicate(item))
            {
                return data.item_ptr_unchecked(i);
            }
        }
        return nullptr;
    }

    template<typename Lambda>
    const T* find_by_predicate(Lambda&& predicate) const
    {
        return const_cast<StackVector*>(this)->find_by_predicate(std::forward(predicate));
    }

    template<typename Lambda>
    size_type find_index_by_predicate(Lambda&& predicate) const
    {
        FatPointer<T> data = get_used_data();
        for (size_type i = 0; i < num_elements; i++)
        {
            if (predicate(data[i]))
            {
                return i;
            }
        }
        return invalid_index;
    }

private:
    // backing implementation for various insert() functions
    inline T* insert_empty_slot(size_t idx)
    {
        const size_t new_num_elements = num_elements + 1;
        if (new_num_elements > capacity())
        {
            reserve(new_num_elements);
        }

        FatPointer<T> data = get_allocated_data();
        for (size_t i = idx; i < new_num_elements; i++)
        {
            data.copy_element(i + 1, i);
        }

        data.zero_element(idx);

        num_elements = new_num_elements;
        return data.item_ptr_unchecked(idx);
    }

public:
    void insert(size_type idx, const T& value)
    {
        T* slot = insert_empty_slot(idx);
        *slot = value;
    }

    void insert(size_type idx, T&& value)
    {
        T* slot = insert_empty_slot(idx);
        *slot = std::move(value);
    }

    template <typename... TArgs>
    T& emplace_insert(size_type idx, TArgs&&... args)
    {
        T* slot = insert_empty_slot(idx);
        new (slot) T(std::forward<TArgs>(args)...);
        return *slot;
    }

    size_type push_back(const T& value)
    {
        const size_t new_num_elements = num_elements + 1;
        if (new_num_elements > capacity())
        {
            reserve(new_num_elements);
        }

        const size_type new_idx = num_elements;
        get_allocated_data()[new_idx] = value;
        num_elements = new_num_elements;
        return new_idx;
    }

    size_type push_back(T&& value)
    {
        const size_t new_num_elements = num_elements + 1;
        if (new_num_elements > capacity())
        {
            reserve(new_num_elements);
        }

        const size_type new_idx = num_elements;
        get_allocated_data()[new_idx] = std::move(value);
        num_elements = new_num_elements;
        return new_idx;
    }

    bool push_back_unique(const T& value)
    {
        if (!contains_value(value))
        {
            push_back(value);
            return true;
        }
        return false;
    }

    bool push_back_unique(T&& value)
    {
        if (!contains_value(value))
        {
            push_back(std::move(value));
            return true;
        }
        return false;
    }

    template <typename... TArgs>
    T& emplace_back(TArgs&&... args)
    {
        const size_t new_num_elements = num_elements + 1;
        const size_t new_index = num_elements;
        if (new_num_elements > capacity())
        {
            reserve(new_num_elements);
        }

        FatPointer<T> data = get_allocated_data();
        data.zero_element(new_index);

        new (data.item_ptr(new_index)) T(std::forward<TArgs>(args)...);
        num_elements = new_num_elements;
        return back();
    }

    template<size_t RhsBufSize, typename RhsAlloc>
    void append_all(const StackVector<T, RhsBufSize, RhsAlloc>& rhs)
    {
        for (size_t i = 0; i < rhs.size(); i++)
        {
            push_back(rhs[i]);
        }
    }

    void remove_at(size_type idx)
    {
        JXC_INDEX_ASSERT(idx, num_elements);

        FatPointer<T> data = get_used_data();
        if constexpr (run_constructors_and_destructors)
        {
            data[idx].~T();
        }

        for (size_t i = idx; i < num_elements - 1; i++)
        {
            data.copy_element(i, i + 1);
        }

        num_elements--;
    }

    void remove_at_swap(size_type idx)
    {
        JXC_INDEX_ASSERT(idx, num_elements);

        FatPointer<T> data = get_used_data();
        const size_t last_index = num_elements - 1;

        if constexpr (run_constructors_and_destructors)
        {
            data[idx].~T();
        }

        if (idx == last_index)
        {
            data.zero_element(idx);
        }
        else
        {
            data.copy_element(idx, last_index);
            data.zero_element(last_index);
        }

        num_elements--;
    }

    void swap_values(size_t idx_a, size_t idx_b)
    {
        JXC_INDEX_ASSERT(idx_a, num_elements);
        JXC_INDEX_ASSERT(idx_b, num_elements);
        if (idx_a == idx_b)
        {
            return;
        }

        FatPointer<T> data = get_used_data();
        data.swap_element(idx_a, idx_b);
    }

    inline T* data()
    {
        return get_allocated_data().ptr;
    }

    inline const T* data() const
    {
        return get_allocated_data().ptr;
    }

    void pop_back()
    {
        JXC_ASSERTF(num_elements > 0, "pop_back() on empty array");
        remove_at(num_elements - 1);
    }

    T& front()
    {
        JXC_ASSERTF(num_elements > 0, "pop_front() on empty array");
        return get_used_data()[0];
    }

    inline const T& front() const
    {
        return const_cast<StackVector*>(this)->front();
    }

    T& back(size_type offset = 1)
    {
        JXC_ASSERTF(num_elements > 0, "back() on empty array");
        JXC_ASSERTF(num_elements >= offset && offset >= 1, "offset out of bounds");
        const size_t idx = static_cast<size_t>(num_elements - offset);
        return get_used_data()[idx];
    }

    inline const T& back(size_t offset = 1) const
    {
        return const_cast<StackVector*>(this)->back(offset);
    }

    /// reverses the entire array by swapping pairs of elements
    void reverse_inplace()
    {
        const size_t sz = num_elements;
        if (sz <= 1)
        {
            return;
        }

        size_t idx_a = 0;
        size_t idx_b = sz - 1;

        // num_pairs uses integer floor division, so if sz is 7, this will give us 3 iterations.
        // this means it works with both odd and even sized arrays.
        const size_t num_pairs = sz / 2;
        for (size_t i = 0; i < num_pairs; i++)
        {
            swap_values(idx_a, idx_b);
            ++idx_a;
            --idx_b;
        }
    }

    template<typename Lambda>
    size_type remove_all_matching_predicate(Lambda&& predicate)
    {
        size_t num_removed = 0;

        if (num_elements == 0)
        {
            return num_removed;
        }
        
        auto data = get_used_data();

        size_t iter = 0;
        size_t data_idx = 0;

        while (iter < num_elements)
        {
            if (predicate(data[data_idx]))
            {
                num_removed++;

                if constexpr (run_constructors_and_destructors)
                {
                    data[data_idx].~T();
                }

                if (data_idx < num_elements - 1)
                {
                    for (size_t i = data_idx; i < num_elements - 1; i++)
                    {
                        data.copy_element(i, i + 1);
                    }
                }

                num_elements--;
                data = get_used_data();
            }
            else
            {
                data_idx++;
            }
            
            iter++;
        }

        return num_removed;
    }

    size_type remove_all(const T& value)
    {
        return remove_all_matching_predicate([&](const T& v) { return v == value; });
    }

    bool remove_first(const T& value)
    {
        const size_type idx = find_index(value);
        if (idx != invalid_index)
        {
            remove_at(idx);
            return true;
        }
        return false;
    }

    bool remove_first_swap(const T& value)
    {
        const size_type idx = find_index(value);
        if (idx != invalid_index)
        {
            remove_at_swap(idx);
            return true;
        }
        return false;
    }

    bool remove_last_swap(const T& value)
    {
        const size_type idx = find_index_from_end(value);
        if (idx != invalid_index)
        {
            remove_at_swap(idx);
            return true;
        }
        return false;
    }
};


#if JXC_HAVE_CONCEPTS
static_assert(traits::ConstructibleFromIterator<StackVector<int, 16>>, "StackVector can be constructed with a begin and end iterator");
#endif


JXC_END_NAMESPACE(detail)

// Byte array that can store a small number of bytes inline without any allocations
using SmallByteArray = detail::StackVector<uint8_t, 16>;

JXC_END_NAMESPACE(jxc)

