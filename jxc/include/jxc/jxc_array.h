#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <variant>
#include "jxc/jxc_core.h"
#include "jxc/jxc_type_traits.h"


namespace jxc::detail
{

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
            JXC_DEBUG_ASSERT(num_elements_removed < arr_capacity);
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

    inline bool push(const T& value)
    {
        if (arr_size >= arr_capacity - 1)
        {
            return false;
        }
        arr[arr_size] = value;
        ++arr_size;
        return true;
    }

    inline bool push(T&& value)
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


// simple fixed-size array that can fall back on an std::vector if it overflows
template<typename T, uint16_t BufSize>
class ArrayBuffer
{
public:
    static constexpr size_t buffer_size = static_cast<size_t>(BufSize);

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
    using ArrType = FixedArray<T, BufSize>;
    using VecType = std::vector<T>;

    std::variant<ArrType, VecType> _data;

    inline bool is_array() const { return _data.index() == 0; }
    inline ArrType& as_array_unchecked() { return std::get<ArrType>(_data); }
    inline const ArrType& as_array_unchecked() const { return std::get<ArrType>(_data); }

    inline bool is_vector() const { return _data.index() == 1; }
    inline VecType& as_vector_unchecked() { return std::get<VecType>(_data); }
    inline const VecType& as_vector_unchecked() const { return std::get<VecType>(_data); }

private:
    template<typename NewType>
    inline void switch_storage_container_to(size_t num_values_to_move, size_t num_extra_slots = 0)
    {
        static_assert(std::is_same_v<NewType, VecType> || std::is_same_v<NewType, ArrType>, "NewType must be ArrType or VecType");
        static constexpr size_t old_variant_index = std::is_same_v<NewType, ArrType> ? 1 : 0;
        using OldType = std::conditional_t<old_variant_index == 0, ArrType, VecType>;
        static_assert(!std::is_same_v<NewType, OldType>, "OldType must not be the same as NewType");
        JXC_DEBUG_ASSERT(_data.index() == old_variant_index);
        NewType new_container;
        new_container.resize(num_values_to_move + num_extra_slots);
        {
            OldType& old_container = std::get<OldType>(_data);

            JXC_DEBUG_ASSERT(num_values_to_move <= old_container.size());

            if constexpr (std::is_trivially_copyable_v<T>)
            {
                memcpy(new_container.data(), &old_container[0], sizeof(T) * num_values_to_move);
            }
            else
            {
                for (size_t i = 0; i < num_values_to_move; i++)
                {
                    new_container[i] = std::move(old_container[i]);
                }
            }
        }
        _data = std::move(new_container);
        JXC_DEBUG_ASSERT(_data.index() == ((old_variant_index == 0) ? 1 : 0));
    }

public:
    ArrayBuffer() : _data(ArrType{}) {}

    ArrayBuffer(const T* src, size_t src_len)
    {
        if (src == nullptr)
        {
            JXC_ASSERT(src_len == 0);
            _data = ArrType();
        }
        else if (src_len <= BufSize)
        {
            _data = ArrType(src, src_len);
        }
        else
        {
            _data = VecType();
            VecType& vec = as_vector_unchecked();
            vec.reserve(src_len);
            for (size_t i = 0; i < src_len; i++)
            {
                vec.push_back(src[i]);
            }
        }
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    ArrayBuffer(const RhsT& rhs)
    {
        const size_t sz = rhs.size();
        if (sz <= BufSize)
        {
            _data = ArrType(rhs.begin(), rhs.end());
        }
        else
        {
            if constexpr (std::is_same_v<RhsT, VecType>)
            {
                _data = rhs;
            }
            else
            {
                _data = VecType(rhs.begin(), rhs.end());
            }
        }
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    ArrayBuffer(RhsT&& rhs)
    {
        const size_t sz = rhs.size();
        if (sz == 0)
        {
            _data = ArrType();
        }
        else if (sz <= BufSize)
        {
            _data = ArrType(std::move(rhs));
        }
        else
        {
            if constexpr (std::is_same_v<RhsT, VecType>)
            {
                _data = std::move(rhs);
            }
            else
            {
                auto vec = VecType();
                vec.reserve(sz);
                const T* rhs_data_ptr = rhs.data();
                for (size_t i = 0; i < sz; i++)
                {
                    vec.push_back(std::move(rhs_data_ptr[i]));
                }
                _data = std::move(vec);
            }
        }
    }

    ArrayBuffer(std::initializer_list<T> values)
    {
        if (values.size() > buffer_size)
        {
            _data = VecType{ values };
        }
        else
        {
            _data = ArrType{ values };
        }
    }

    // not an ideal constructor - without knowing the size in advance, we can't pick one container type and stick to it
    template<typename IterType>
    ArrayBuffer(IterType iter_start, IterType iter_end)
    {
        _data = ArrType{};
        IterType iter = iter_start;
        while (iter != iter_end)
        {
            push(*iter);
            ++iter;
        }
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    ArrayBuffer& operator=(const RhsT& rhs)
    {
        if (this != &rhs)
        {
            const size_t sz = rhs.size();
            if (sz <= BufSize)
            {
                _data = ArrType(rhs);
            }
            else
            {
                if constexpr (std::is_same_v<RhsT, VecType>)
                {
                    _data = rhs;
                }
                else
                {
                    _data = VecType(rhs.data(), rhs.data() + sz);
                }
            }
        }
        return *this;
    }

    template<JXC_CONCEPT(traits::Array<T>) RhsT>
    ArrayBuffer& operator=(RhsT&& rhs)
    {
        if (this != &rhs)
        {
            const size_t sz = rhs.size();
            if (sz <= BufSize)
            {
                _data = ArrType(std::move(rhs));
            }
            else
            {
                if constexpr (std::is_same_v<RhsT, VecType>)
                {
                    _data = std::move(rhs);
                }
                else
                {
                    auto vec = VecType();
                    vec.reserve(sz);
                    const T* rhs_data_ptr = rhs.data();
                    for (size_t i = 0; i < sz; i++)
                    {
                        vec.push_back(std::move(rhs_data_ptr[i]));
                    }
                    _data = std::move(vec);
                }
            }
        }
        return *this;
    }

    ArrayBuffer(const ArrayBuffer&) = default;
    ArrayBuffer(ArrayBuffer&&) = default;
    ArrayBuffer& operator=(const ArrayBuffer&) = default;
    ArrayBuffer& operator=(ArrayBuffer&&) = default;

    inline size_t size() const { return is_array() ? as_array_unchecked().size() : as_vector_unchecked().size(); }

    inline T* data() { return is_array() ? as_array_unchecked().data() : as_vector_unchecked().data(); }
    inline const T* data() const { return is_array() ? as_array_unchecked().data() : as_vector_unchecked().data(); }

    PointerIterator<T> begin() { return PointerIterator<T>{ data() }; }
    PointerIterator<T> end() { return PointerIterator<T>{ data() + size() }; }

    ConstPointerIterator<T> begin() const { return PointerIterator<T>{ data() }; }
    ConstPointerIterator<T> end() const { return PointerIterator<T>{ data() + size() }; }

    inline void clear(bool full_clear = true)
    {
        if (is_vector())
        {
            as_vector_unchecked().clear();

            if (full_clear)
            {
                // reset back to an array type to dump all allocated memory
                _data = ArrType();
            }
        }
        else
        {
            as_array_unchecked().clear();
        }
    }

    void resize(size_t new_len, bool auto_shrink = true, bool auto_zero = true)
    {
        if (is_vector())
        {
            if (auto_shrink && new_len <= buffer_size)
            {
                // new size fits in the array, move the data over there
                switch_storage_container_to<ArrType>(std::min(new_len, as_vector_unchecked().size()));
            }
            else
            {
                as_vector_unchecked().resize(new_len);
            }
        }
        else
        {
            if (new_len > buffer_size)
            {
                // new size is larger than the max array capacity, move the data to a vector
                const size_t orig_size = as_array_unchecked().size();
                const size_t num_new_slots = new_len - orig_size;
                switch_storage_container_to<VecType>(orig_size, num_new_slots);
            }
            else
            {
                as_array_unchecked().resize(new_len, auto_zero);
            }
        }
    }

    inline T& operator[](size_t idx)
    {
        if (is_array())
        {
            JXC_DEBUG_ASSERT(idx < as_array_unchecked().size());
            return as_array_unchecked()[idx];
        }
        else
        {
            JXC_DEBUG_ASSERT(idx < as_vector_unchecked().size());
            return as_vector_unchecked()[idx];
        }
    }

    inline const T& operator[](size_t idx) const { return const_cast<ArrayBuffer*>(this)->operator[](idx); }

    inline T& front()
    {
        if (is_array())
        {
            JXC_ASSERT(as_array_unchecked().size() > 0);
            return as_array_unchecked()[0];
        }
        else
        {
            JXC_ASSERT(as_vector_unchecked().size() > 0);
            return as_vector_unchecked()[0];
        }
    }

    inline T& back()
    {
        if (is_array())
        {
            JXC_ASSERT(as_array_unchecked().size() > 0);
            return as_array_unchecked().back();
        }
        else
        {
            JXC_ASSERT(as_vector_unchecked().size() > 0);
            return as_vector_unchecked().back();
        }
    }

    inline const T& front() const { return const_cast<ArrayBuffer*>(this)->front(); }
    inline const T& back() const { return const_cast<ArrayBuffer*>(this)->back(); }

    T& push(const T& value)
    {
        if (is_vector())
        {
            as_vector_unchecked().push_back(value);
            return as_vector_unchecked().back();
        }

        JXC_DEBUG_ASSERT(is_array());
        if (as_array_unchecked().size() < buffer_size - 1)
        {
            JXC_UNUSED_IN_RELEASE const bool success = as_array_unchecked().push(value);
            JXC_DEBUG_ASSERT(success);
            return as_array_unchecked().back();
        }
        else
        {
            const size_t orig_size = as_array_unchecked().size();
            switch_storage_container_to<VecType>(orig_size, 1);
            JXC_DEBUG_ASSERT(is_vector());
            JXC_DEBUG_ASSERT(as_vector_unchecked().size() == orig_size + 1);
            as_vector_unchecked()[orig_size] = value;
            return as_vector_unchecked().back();
        }
    }

    T& push(T&& value)
    {
        if (is_vector())
        {
            as_vector_unchecked().push_back(std::move(value));
            return as_vector_unchecked().back();
        }

        JXC_DEBUG_ASSERT(is_array());
        if (as_array_unchecked().size() < buffer_size - 1)
        {
            JXC_UNUSED_IN_RELEASE const bool success = as_array_unchecked().push(std::move(value));
            JXC_DEBUG_ASSERT(success);
            return as_array_unchecked().back();
        }
        else
        {
            const size_t orig_size = as_array_unchecked().size();
            switch_storage_container_to<VecType>(orig_size, 1);
            JXC_DEBUG_ASSERT(is_vector());
            JXC_DEBUG_ASSERT(as_vector_unchecked().size() == orig_size + 1);
            as_vector_unchecked()[orig_size] = std::move(value);
            return as_vector_unchecked().back();
        }
    }

    bool pop_back()
    {
        if (is_array())
        {
            return (as_array_unchecked().size() > 0) ? as_array_unchecked().pop_back() : false;
        }

        if (as_vector_unchecked().size() > 0)
        {
            as_vector_unchecked().pop_back();
            return true;
        }

        return false;
    }
};


#if JXC_HAVE_CONCEPTS
static_assert(traits::ConstructibleFromIterator<FixedArray<int, 16>>, "FixedArray can be constructed with a begin and end iterator");
static_assert(traits::ConstructibleFromIterator<ArrayBuffer<int, 16>>, "ArrayBuffer can be constructed with a begin and end iterator");
#endif

} // namespace jxc::detail

namespace jxc
{

// Byte array that can store a small number of bytes inline without any allocations
using SmallByteArray = detail::ArrayBuffer<uint8_t, 16>;

} // namespace jxc

