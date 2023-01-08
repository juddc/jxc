#pragma once
#include "jxc/jxc_core.h"
#include "jxc/jxc_type_traits.h"


JXC_BEGIN_NAMESPACE(jxc)


// similar to string_view, but for byte arrays
class BytesView
{
public:
    using value_type = uint8_t;

private:
    const value_type* ptr = nullptr;
    size_t len = 0;

public:
    BytesView() = default;
    BytesView(const value_type* ptr, size_t len) : ptr(ptr), len(len) {}

    template<JXC_CONCEPT(traits::HasDataAndSize<value_type>) RhsT>
    BytesView(const RhsT& rhs) : ptr(rhs.data()), len(rhs.size()) {}

    BytesView(const BytesView&) = default;
    BytesView& operator=(const BytesView&) = default;

    explicit inline operator bool() const { return ptr != nullptr; }
    inline bool is_empty() const { return ptr == nullptr || len == 0; }
    inline bool is_valid_index(size_t idx) const { return ptr != nullptr && idx < len; }

    inline const value_type* data() const { return ptr; }
    inline size_t size() const { return len; }

    inline value_type& operator[](size_t idx) { JXC_ASSERT(is_valid_index(idx)); return const_cast<value_type&>(ptr[idx]); }
    inline const value_type& operator[](size_t idx) const { JXC_ASSERT(is_valid_index(idx)); return ptr[idx]; }

    inline BytesView slice(size_t start_idx = 0, size_t count = invalid_idx) const
    {
        if (ptr == nullptr || len == 0 || start_idx >= len) { return BytesView{}; }
        return BytesView{ ptr + start_idx, std::min<size_t>(count, len - start_idx) };
    }

    bool operator==(const BytesView& rhs) const
    {
        if (len != rhs.len) { return false; }
        if (len == 0) { return true; }
        if (ptr == nullptr || rhs.ptr == nullptr) { return ptr == rhs.ptr; }
        if constexpr (std::is_trivially_copyable_v<value_type>)
        {
            return memcmp(ptr, rhs.ptr, len) == 0;
        }
        else
        {
            for (size_t i = 0; i < len; i++)
            {
                if (ptr[i] != rhs.ptr[i])
                {
                    return false;
                }
            }
            return true;
        }
    }

    inline bool operator!=(const BytesView& rhs) const { return !operator==(rhs); }

    template<JXC_CONCEPT(traits::HasDataAndSize<value_type>) RhsT>
    inline bool operator==(const RhsT& rhs) const { return operator==(BytesView{ rhs.data(), rhs.size() }); }

    template<JXC_CONCEPT(traits::HasDataAndSize<value_type>) RhsT>
    inline bool operator!=(const RhsT& rhs) const { return !operator==(rhs); }
};


#if JXC_HAVE_CONCEPTS
static_assert(traits::Bytes<BytesView>, "BytesView matches the Bytes trait");
#endif


JXC_BEGIN_NAMESPACE(traits)

// convert any type that has data() and size() members to a BytesView
template<JXC_CONCEPT(HasDataAndSize<uint8_t>) T>
constexpr inline BytesView cast_bytes_to_view(const T& val)
{
    return BytesView{ val.data(), val.size() };
}

template<typename T>
static constexpr bool is_unicode_char = std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>
#if JXC_CPP20
    || std::is_same_v<T, char8_t>
#endif
    ;

JXC_END_NAMESPACE(traits)

JXC_BEGIN_NAMESPACE(detail)

// Simple stack-based char buffer, generally intended for small strings, but could be used for anything
template<typename T, size_t BufSize>
struct MiniBuffer
{
    using value_type = T;
    static_assert(std::is_integral_v<value_type> || std::is_same_v<value_type, char> || std::is_same_v<value_type, wchar_t> || traits::is_unicode_char<value_type>,
            "MiniBuffer requires a character or integer type");

    static constexpr size_t buf_size = BufSize;
    value_type buf[buf_size] = { 0 };

    inline value_type& operator[](size_t idx) { JXC_ASSERT(idx < buf_size); return buf[idx]; }
    inline const value_type& operator[](size_t idx) const { JXC_ASSERT(idx < buf_size); return buf[idx]; }

    inline void clear() { memset(&buf[0], 0, buf_size); }
    inline value_type* data() { return &buf[0]; }
    inline const value_type* data() const { return &buf[0]; }
    inline size_t capacity() const { return buf_size; }
    //inline size_t size() const { return buf_size; }

    inline size_t string_length() const
    {
        if constexpr (std::is_same_v<value_type, char>)
        {
            return strnlen(&buf[0], buf_size);
        }
        else if constexpr (std::is_same_v<value_type, wchar_t>)
        {
            return wcsnlen(&buf[0], buf_size);
        }
        else if constexpr (traits::is_unicode_char<value_type>)
        {
            size_t len = 0;
            while (len < buf_size)
            {
                if (buf[len] == 0) { break; }
                ++len;
            }
            return len;
        }
        else
        {
            return buf_size;
        }
    }

    inline auto as_view(size_t view_size = invalid_idx) const
    {
        if (view_size > buf_size)
        {
            view_size = string_length();
        }

        if constexpr (std::is_same_v<value_type, char>)
        {
            return std::string_view{ data(), view_size };
        }
        else if constexpr (std::is_same_v<value_type, wchar_t>)
        {
            return std::wstring_view{ data(), view_size };
        }
#if JXC_CPP20
        else if constexpr (std::is_same_v<value_type, char8_t>)
        {
            return std::u8string_view{ data(), view_size };
        }
#endif
        else if constexpr (std::is_same_v<value_type, char16_t>)
        {
            return std::u16string_view{ data(), view_size };
        }
        else if constexpr (std::is_same_v<value_type, char32_t>)
        {
            return std::u32string_view{ data(), view_size };
        }
        else if constexpr (std::is_same_v<value_type, uint8_t>)
        {
            return BytesView{ data(), view_size };
        }
        else
        {
            []<bool Err = true>() { static_assert(!Err, "Invalid type for as_view()"); };
            return nullptr;
        }
    }
};

JXC_END_NAMESPACE(detail)
JXC_END_NAMESPACE(jxc)
