#pragma once
#include <sstream>
#include "jxc/jxc_core.h"
#include "jxc/jxc_memory.h"


JXC_BEGIN_NAMESPACE(jxc)


// Can contain an owned string, a string view, or an inline string (if the string is small enough)
//
// The reason this class exists is because we want to allow returning strings that were parsed from
// a JXC string buffer as views into that buffer, to avoid potentially thousands or even millions
// of extraneous string copies that were not needed. However, any string that needs to be encoded
// (eg. any string with escapes like '\n') needs to be copied into an owned string.
//
class FlexString
{
public:
    static constexpr size_t max_inline_len = 23;

    struct ConstructView{};
    struct ConstructViewStatic{};
    struct ConstructOwned{};
    struct ConstructInline{};

private:
    enum Type : uint8_t
    {
        // string is stored in an inline buffer
        FlexInline = 0,

        // string is a pointer and length
        FlexView,

        // string is a view into static memory (constructed from a string literal), so we can pretend to not be a view
        FlexViewStatic,

        // string is an owned block of dynamic memory
        FlexOwned,
    };

    static constexpr size_t str_buf_size = max_inline_len + 1;

    // string length, regardless of storage mechanism
    size_t flex_len = 0;

    // string data storage union
    union
    {
        const char* str_data;
        char str_buf[str_buf_size];
    };

    // String storage type
    Type type = FlexInline;

    // Assumes this string is a valid, owned, dynamically allocated string, and frees it
    void free_owned();

    // Set this string to the empty string
    inline void set_inline_empty_internal()
    {
        type = FlexInline;
        flex_len = 0;
        // clear 8 bytes of the inline buffer instead of one (eg. str_buf[0] = '\0')
        str_data = nullptr;
    }

    // Set this string to an inline string (requires that it fits in the inline buffer!)
    void set_inline_internal(const char* in_str, size_t in_str_len);

    // Set this string to be a view (just a pointer and length)
    void set_view_internal(const char* in_str, size_t in_str_len);

    // Set this string to be a view into a static string literal
    void set_view_static_internal(const char* in_str, size_t in_str_len);

    // Set this string to be an owned copy of the input string
    void set_owned_internal(const char* in_str, size_t in_str_len);

    // Set this string to be an owned empty buffer.
    // Returns a non-const pointer to the buffer for copying data into it.
    char* set_owned_empty_buffer_internal(size_t in_buffer_len, char fill_char = '\0');

    // Copy another string into this one
    void copy_internal(const FlexString& rhs);

    // Move ownership of another string into this one
    void move_internal(FlexString&& rhs);

public:
    FlexString() noexcept
    {
        set_inline_empty_internal();
    }

    // Default constructor, implicitly converts from string_view.
    // This constructor always defaults to either an inline string, or an owned allocated string
    // (depending on the size), because constructing a FlexString as a view should always be
    // explicit (we never want an accidental string view that gets free'd out from under us)
    FlexString(std::string_view in_str) noexcept
    {
        const size_t sz = in_str.size();
        if (sz == 0)
        {
            set_inline_empty_internal();
        }
        else if (sz <= max_inline_len)
        {
            set_inline_internal(in_str.data(), sz);
        }
        else
        {
            set_owned_internal(in_str.data(), sz);
        }
    }

    // Constructor that takes a string literal. In this case we can store it as a view but pretend
    // it's "owned" because we don't need to worry about it getting free'd
    template<size_t N>
    FlexString(const char(&in_static_str)[N], ConstructViewStatic) noexcept
        : flex_len(N)
        , str_data(in_static_str)
        , type(FlexViewStatic)
    {
    }

    // Constructor that takes a c-string and a length, and stores it as a view
    FlexString(const char* in_str, size_t in_len, ConstructView) noexcept
        : flex_len(in_len)
        , str_data(in_str)
        , type(FlexView)
    {
    }

    // Constructor that takes a string_view and stores it as a view
    FlexString(std::string_view str, ConstructView) noexcept
        : flex_len(str.size())
        , str_data(str.data())
        , type(FlexView)
    {
    }

    // Constructor that takes a c-string and a length, duplicates the string, and stores the copy
    // as an owned string (will free the copy on destruction).
    // Note that this constructor will always allocate a copy, regardless of the input string's size.
    // (It will never store the string as inline even if it would fit in the buffer).
    FlexString(const char* in_str, size_t in_len, ConstructOwned) noexcept
        : flex_len(in_len)
        , str_data(JXC_STRNDUP(in_str, in_len))
        , type(FlexOwned)
    {
    }

    // Constructor that takes a string-view, duplicates the string, and stores the copy as an owned
    // string (will free the copy on destruction).
    // Note that this constructor will always allocate a copy, regardless of the input string's size.
    // (It will never store the string as inline even if it would fit in the buffer).
    FlexString(std::string_view str, ConstructOwned) noexcept
        : flex_len(str.size())
        , str_data(JXC_STRNDUP(str.data(), str.size()))
        , type(FlexOwned)
    {
    }

    // Constructor that forces inline storage.
    // This will clamp the input string length to always fit in the inline buffer.
    // Warning: any data that does not fit in the inline buffer will be discarded!
    FlexString(std::string_view str, ConstructInline) noexcept
        : flex_len(str.size())
        , type(FlexInline)
    {
        if (str.data() != nullptr)
        {
            set_inline_internal(str.data(), std::min(str.size(), max_inline_len));
        }
        else
        {
            str_buf[0] = '\0';
        }
    }

    ~FlexString() noexcept
    {
        if (type == FlexOwned)
        {
            free_owned();
        }
    }

    FlexString(const FlexString& rhs) noexcept
    {
        copy_internal(rhs);
    }

    FlexString(FlexString&& rhs) noexcept
    {
        move_internal(std::move(rhs));
    }

    FlexString& operator=(const FlexString& rhs) noexcept
    {
        if (this != &rhs)
        {
            if (type == FlexOwned)
            {
                free_owned();
            }
            copy_internal(rhs);
        }
        return *this;
    }

    FlexString& operator=(FlexString&& rhs) noexcept
    {
        if (this != &rhs)
        {
            if (type == FlexOwned)
            {
                free_owned();
            }
            move_internal(std::move(rhs));
        }
        return *this;
    }

    static inline FlexString make_inline(std::string_view str)
    {
        return FlexString{ str, ConstructInline{} };
    }

    static inline FlexString make_view(std::string_view str)
    {
        return FlexString{ str, ConstructView{} };
    }

    static inline FlexString make_owned(std::string_view str)
    {
        return FlexString{ str, ConstructOwned{} };
    }

    template<size_t N>
    static inline FlexString from_literal(const char(&str)[N])
    {
        return FlexString(str, N, ConstructViewStatic{});
    }

    void set_owned(const char* in_str, size_t in_str_len)
    {
        if (type == FlexOwned)
        {
            free_owned();
        }

        if (in_str_len == 0 || in_str == nullptr)
        {
            set_inline_empty_internal();
        }
        else if (in_str_len <= max_inline_len)
        {
            set_inline_internal(in_str, in_str_len);
        }
        else
        {
            set_owned_internal(in_str, in_str_len);
        }
    }

    inline bool operator==(const FlexString& rhs) const { return as_view() == rhs.as_view(); }
    inline bool operator!=(const FlexString& rhs) const { return as_view() != rhs.as_view(); }

    inline bool operator==(std::string_view rhs) const { return as_view() == rhs; }
    inline bool operator!=(std::string_view rhs) const { return as_view() != rhs; }

    inline bool operator==(const char* rhs) const { return as_view() == rhs; }
    inline bool operator!=(const char* rhs) const { return as_view() != rhs; }

    // resets this to an empty string
    inline void reset()
    {
        if (type == FlexOwned)
        {
            free_owned();
        }
        set_inline_empty_internal();
    }

    // Returns a pointer to the beginning of the contained string
    char* data();

    // Returns a pointer to the beginning of the contained string
    inline const char* data() const { return const_cast<FlexString*>(this)->data(); }

    // Returns the length of the contained string
    inline size_t size() const { return flex_len; }

    // Resizes the string buffer, filling with the fill character if needed.
    // Calling resize will ALWAYS result in the string being inline or owned, even if the size is the same.
    void resize(size_t new_size, char fill_char = '\0');

    // Checks if this FlexString is a string view (and does not own its data)
    inline bool is_view() const { return type == FlexView; }

    // Returns a view into the contained string, regardless of how it is stored
    inline std::string_view as_view() const { return std::string_view{ data(), flex_len }; }

    // Implicit conversion to string_view
    inline operator std::string_view() const { return as_view(); }

    // Returns a FlexString that's an owned copy of this one
    inline FlexString to_owned() const { return FlexString(as_view()); }

    // If this FlexString is a view, converts it in-place to an owned or inline string
    void to_owned_inplace();

    // Gets the first char of the string
    inline char front() const { return (flex_len > 0) ? data()[0] : '\0'; }

    // Gets the first char of the string
    inline char& front() { JXC_ASSERT(flex_len > 0); return data()[0]; }

    // Gets the last char of the string
    inline char back() const { return (flex_len > 0) ? data()[flex_len - 1] : '\0'; }

    // Gets the first char of the string
    inline char& back() { JXC_ASSERT(flex_len > 0); return data()[flex_len - 1]; }

    // Gets a character inside the string (UNCHECKED in release builds)
    inline char& operator[](size_t idx) { JXC_DEBUG_ASSERT(idx < flex_len); return const_cast<char&>(data()[idx]); }
    inline const char& operator[](size_t idx) const { JXC_DEBUG_ASSERT(idx < flex_len); return data()[idx]; }

    // Gets a character inside the string (checked)
    inline char& at(size_t idx)
    {
        JXC_ASSERTF(idx < flex_len, "Index {} out of range for string with size {}", idx, flex_len);
        return data()[idx];
    }

    // Gets a character inside the string (checked)
    inline const char& at(size_t idx) const
    {
        JXC_ASSERTF(idx < flex_len, "Index {} out of range for string with size {}", idx, flex_len);
        return data()[idx];
    }

    // operator<< overload so FlexString can be used with std::cout, std::ostringstream, and friends.
    friend inline std::ostream& operator<<(std::ostream& os, const FlexString& value)
    {
        return os << value.as_view();
    }
};

JXC_END_NAMESPACE(jxc)

