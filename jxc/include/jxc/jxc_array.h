#pragma once
#include <cstdint>
#include <array>
#include "jxc/jxc_core.h"
#include "jxc/jxc_type_traits.h"


JXC_BEGIN_NAMESPACE(jxc)
JXC_BEGIN_NAMESPACE(detail)

// simple fixed-size array with a defined size
template<typename T, uint16_t MaxSize>
class FixedArray
{
public:
    static constexpr size_t arr_capacity = static_cast<size_t>(MaxSize);

    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = PointerIterator<T>;
    using const_iterator = ConstPointerIterator<T>;

private:
    static constexpr bool arr_size_fits_in_uint8 = arr_capacity <= static_cast<size_t>(std::numeric_limits<uint8_t>::max());

public:
    using size_type_internal = std::conditional_t<arr_size_fits_in_uint8, uint8_t, uint16_t>;

    static_assert(arr_capacity <= std::numeric_limits<size_type_internal>::max(),
        "FixedArray capacity does not fit in its size type");

    //static_assert(std::is_default_constructible_v<T>, "FixedArray requires a default constructible type");

private:
    size_type_internal arr_size = 0;
    std::array<T, arr_capacity> arr = {};

private:
    void clear_slice(size_t start_idx, size_t num_elements)
    {
        JXC_DEBUG_ASSERT(num_elements > 0);
        JXC_DEBUG_ASSERT(start_idx + num_elements - 1 < arr_capacity);
        if constexpr (std::is_trivially_destructible_v<T>)
        {
            memset(&arr[start_idx], 0, sizeof(T) * num_elements);
        }
        else
        {
            for (size_t i = start_idx; i < num_elements; i++)
            {
                arr[i] = T();
            }
        }
    }

    void copy_into(const T* rhs_data, size_t rhs_len)
    {
        JXC_DEBUG_ASSERT(rhs_len > 0);
        JXC_DEBUG_ASSERT(rhs_len <= arr_capacity);
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            memcpy(&arr[0], rhs_data, sizeof(T) * rhs_len);
        }
        else
        {
            for (size_t i = 0; i < rhs_len; i++)
            {
                arr[i] = rhs_data[i];
            }
        }
    }

    void move_into(T* rhs_data, size_t rhs_len)
    {
        JXC_DEBUG_ASSERT(rhs_len > 0);
        JXC_DEBUG_ASSERT(rhs_len <= arr_capacity);
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            memcpy(&arr[0], rhs_data, sizeof(T) * rhs_len);
        }
        else
        {
            for (size_t i = 0; i < rhs_len; i++)
            {
                arr[i] = std::move(rhs_data[i]);
            }
        }
    }

public:
    FixedArray() = default;

    FixedArray(const T* rhs, size_t rhs_len)
        : arr_size(static_cast<size_type_internal>(rhs_len))
    {
        if (rhs_len > 0 && rhs != nullptr)
        {
            JXC_ASSERT(rhs_len <= arr_capacity);
            copy_into(rhs, rhs_len);
        }
        else
        {
            clear_slice(0, arr_capacity);
        }
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    FixedArray(const RhsT& rhs)
    {
        const size_t sz = rhs.size();
        JXC_ASSERT(sz <= arr_capacity);
        copy_into(rhs.data(), sz);
        arr_size = static_cast<size_type_internal>(sz);
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    FixedArray(RhsT&& rhs)
    {
        const size_t sz = rhs.size();
        JXC_ASSERT(sz <= arr_capacity);
        move_into(rhs.data(), sz);
        arr_size = static_cast<size_type_internal>(sz);
    }

    /// This constructor will copy items from the iterator until we run out of items, or we run out of space
    template<typename IterType>
    FixedArray(IterType iter_start, IterType iter_end)
    {
        IterType iter = iter_start;
        while (iter != iter_end)
        {
            if (!push(*iter))
            {
                break;
            }
            ++iter;
        }
    }

    FixedArray(std::initializer_list<T> values)
        : arr_size(static_cast<size_type_internal>(values.size()))
    {
        JXC_ASSERT(values.size() <= arr_capacity);
        size_t idx = 0;
        for (const auto& v : values)
        {
            arr[idx] = v;
            ++idx;
        }
    }

    FixedArray(const FixedArray&) = default;
    FixedArray(FixedArray&&) = default;
    FixedArray& operator=(const FixedArray&) = default;
    FixedArray& operator=(FixedArray&&) = default;

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    FixedArray& operator=(const RhsT& rhs)
    {
        if (this != &rhs)
        {
            clear();
            const size_t sz = rhs.size();
            JXC_ASSERT(sz <= arr_capacity);
            copy_into(rhs.data(), sz);
            arr_size = sz;
        }
        return *this;
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    FixedArray& operator=(RhsT&& rhs)
    {
        if (this != &rhs)
        {
            clear();
            const size_t sz = rhs.size();
            JXC_ASSERT(sz <= arr_capacity);
            move_into(rhs.data(), sz);
            arr_size = sz;
        }
        return *this;
    }

    FixedArray& operator=(std::initializer_list<T> values)
    {
        JXC_ASSERT(values.size() <= arr_capacity);
        clear();
        arr_size = values.size();
        for (size_t i = 0; i < values.size(); i++)
        {
            arr[i] = values[i];
        }
    }

    PointerIterator<T> begin() { return PointerIterator<T>{ &arr[0] }; }
    PointerIterator<T> end() { return PointerIterator<T>{ &arr[0] + arr_size }; }

    ConstPointerIterator<T> begin() const { return PointerIterator<T>{ &arr[0] }; }
    ConstPointerIterator<T> end() const { return PointerIterator<T>{ &arr[0] + arr_size }; }

    inline size_t size() const { return (size_t)arr_size; }
    inline size_t max_size() const { return arr_capacity; }

    inline void resize(size_t new_size, bool autozero = false)
    {
        JXC_ASSERT(new_size <= arr_capacity);
        const size_type_internal orig_size = arr_size;
        arr_size = static_cast<size_type_internal>(new_size);
        if (autozero && arr_size > orig_size)
        {
            const size_t num_elements_removed = arr_size - orig_size;
            JXC_DEBUG_ASSERT(num_elements_removed <= arr_capacity);
            clear_slice(orig_size, num_elements_removed);
        }
    }

    inline T* data() { return arr.data(); }
    inline const T* data() const { return arr.data(); }

    inline void clear()
    {
        if (arr_size > 0)
        {
            clear_slice(0, arr_size);
            arr_size = 0;
        }
    }

    inline void remove_at(size_t idx)
    {
        JXC_ASSERT(idx < arr_size);
        if (arr_size == 1)
        {
            clear();
            return;
        }

        // swap all items from idx to the end of the array, then shrink the array size by 1
        size_t i = idx;
        const size_t last_idx = arr_size - 1;
        while (i < last_idx)
        {
            std::swap(arr[i], arr[i + 1]);
            ++i;
        }
        --arr_size;
    }

    inline void remove_at_swap(size_t idx)
    {
        JXC_ASSERT(idx < arr_size);
        if (arr_size == 1)
        {
            clear();
            return;
        }

        // swap the item at the specified index with the item at the end of the array, then shrink the array size by 1
        const size_t last_idx = arr_size - 1;
        if (idx != last_idx)
        {
            std::swap(arr[idx], arr[last_idx]);
        }
        --arr_size;
    }

    inline T& operator[](size_t idx) { JXC_DEBUG_ASSERT(idx < arr_size); return arr[idx]; }
    inline const T& operator[](size_t idx) const { JXC_DEBUG_ASSERT(idx < arr_size); return arr[idx]; }

    inline T& front() { JXC_ASSERT(arr_size > 0); return arr[0]; }
    inline T& back() { JXC_ASSERT(arr_size > 0); return arr[arr_size - 1]; }

    inline const T& front() const { return reinterpret_cast<FixedArray*>(this)->front(); }
    inline const T& back() const { return reinterpret_cast<FixedArray*>(this)->back(); }

    inline bool push_back(const T& value)
    {
        if (arr_size >= arr_capacity - 1)
        {
            return false;
        }
        arr[arr_size] = value;
        ++arr_size;
        return true;
    }

    inline bool push_back(T&& value)
    {
        if (arr_size >= arr_capacity - 1)
        {
            return false;
        }
        arr[arr_size] = std::move(value);
        ++arr_size;
        return true;
    }

    inline bool pop_back()
    {
        if (arr_size > 0)
        {
            --arr_size;
            if constexpr (!std::is_trivially_copyable_v<T>)
            {
                arr[arr_size] = T();
            }
            return true;
        }
        return false;
    }
};


#if JXC_HAVE_CONCEPTS
static_assert(traits::ConstructibleFromIterator<FixedArray<int, 16>>, "FixedArray can be constructed with a begin and end iterator");
#endif

JXC_END_NAMESPACE(detail)
JXC_END_NAMESPACE(jxc)

