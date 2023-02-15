#pragma once
#ifndef _JXC_CORE_H_
#define _JXC_CORE_H_
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <functional>
#include <string>
#include <string_view>
#include <memory>
#include <algorithm>
#include <type_traits>
#include <utility>

#include "jxc/jxc_format.h"

#define JXC_VERSION_MAJOR 0
#define JXC_VERSION_MINOR 9
#define JXC_VERSION_PATCH 0

#if !defined(JXC_ENABLE_RELEASE_ASSERTS)
#define JXC_ENABLE_RELEASE_ASSERTS 1
#endif

#if !defined(JXC_ENABLE_DEBUG_ASSERTS)
#define JXC_ENABLE_DEBUG_ASSERTS 1
#endif

#if !defined(JXC_ENABLE_DEBUG_ALLOCATOR)
#define JXC_ENABLE_DEBUG_ALLOCATOR 0
#endif

#if !defined(JXC_ENABLE_JUMP_BLOCK_PROFILER)
#define JXC_ENABLE_JUMP_BLOCK_PROFILER 0
#endif

#ifndef JXC_DEBUG
#define JXC_DEBUG 1
#endif

#if __cplusplus > 201703L
#define JXC_CPP20 1
#else
#define JXC_CPP20 0
#endif

// Namespace macros to prevent IDEs from trying to indent them.
#if !defined(JXC_BEGIN_NAMESPACE)
#define JXC_BEGIN_NAMESPACE(NAME) namespace NAME {
#endif
#if !defined(JXC_END_NAMESPACE)
#define JXC_END_NAMESPACE(NAME) }
#endif

#if !JXC_DEBUG && !defined(JXC_FORCEINLINE)
#if defined(_MSC_VER)
#define JXC_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define JXC_FORCEINLINE __attribute__((always_inline)) inline
#endif
#endif

#ifndef JXC_FORCEINLINE
#define JXC_FORCEINLINE inline
#endif

#ifndef JXC_NOEXPORT
#if defined(__GNUC__)
#define JXC_NOEXPORT __attribute__ ((visibility("hidden")))
#else
#define JXC_NOEXPORT
#endif
#endif

#ifndef JXC_UNUSED_IN_RELEASE
#if JXC_DEBUG
#define JXC_UNUSED_IN_RELEASE
#else
#define JXC_UNUSED_IN_RELEASE [[maybe_unused]]
#endif
#endif

#include "jxc/jxc_memory.h"

#ifndef JXC_LOG
#define JXC_LOG(LEVEL, FMTSTRING, ...) _jxc_log_message_string(::jxc::LogLevel::LEVEL, ::jxc::format(FMTSTRING "\n", ##__VA_ARGS__))
#endif

#ifndef JXC_ERROR
#define JXC_ERROR(FMTSTRING, ...) _jxc_log_message_string(::jxc::LogLevel::Error, ::jxc::format(FMTSTRING "\n", ##__VA_ARGS__))
#endif

#if JXC_ENABLE_RELEASE_ASSERTS
#if !defined(JXC_ASSERT)
#define JXC_ASSERT(COND) do { if (!(COND)) { _jxc_assert_failed(__FILE__, __LINE__, (#COND)); } } while(0)
#endif

#if !defined(JXC_ASSERTF)
#define JXC_ASSERTF(COND, MSG, ...) do { if (!(COND)) { _jxc_assert_failed_msg(__FILE__, __LINE__, (#COND), ::jxc::format(MSG, ##__VA_ARGS__)); } } while(0)
#endif
#endif

#if JXC_ENABLE_DEBUG_ASSERTS && JXC_DEBUG
#if !defined(JXC_DEBUG_ASSERT)
#define JXC_DEBUG_ASSERT(...) JXC_ASSERT(__VA_ARGS__)
#endif

#if !defined(JXC_DEBUG_ASSERTF)
#define JXC_DEBUG_ASSERTF(COND, MSG, ...) JXC_ASSERTF(COND, MSG, ##__VA_ARGS__)
#endif
#endif

#if !defined(JXC_UNREACHABLE)
#if defined(__GNUC__) || defined(__clang__)
#define JXC_UNREACHABLE(...) do { JXC_ASSERTF(false, ##__VA_ARGS__); __builtin_unreachable(); } while(0)
#elif defined(_MSC_VER)
#define JXC_UNREACHABLE(...) do { JXC_ASSERTF(false, ##__VA_ARGS__); __assume(0); } while(0)
#else
#define JXC_UNREACHABLE(...) JXC_ASSERTF(false, ##__VA_ARGS__)
#endif
#endif

#ifndef JXC_ENUMSTR
#define JXC_ENUMSTR(ENUMTYPE, NAME) ENUMTYPE::NAME: return #NAME
#endif

#if !defined(JXC_COUNT_LEADING_ZEROS_U32)
#if defined(__GNUC__) || defined(__clang__)
#define JXC_COUNT_LEADING_ZEROS_U32(VAL) __builtin_clz(VAL)
#elif JXC_CPP20
#define JXC_COUNT_LEADING_ZEROS_U32(VAL) std::countl_zero<uint32_t>(VAL)
#elif defined(_MSC_VER)
JXC_FORCEINLINE int32_t _jxc_internal_msvc_count_leading_zeros(uint32_t value)
{
    unsigned long index;
    _BitScanReverse(&index, value);
    return (int32_t)(31 ^ index);
}
#define JXC_COUNT_LEADING_ZEROS_U32(VAL) _jxc_internal_msvc_count_leading_zeros(VAL)
#else
#error JXC_COUNT_LEADING_ZEROS_U32 not implemented for this compiler or platform
#endif
#endif

// fallbacks - make sure these macros are always defined
#if !defined(JXC_ASSERT)
#define JXC_ASSERT(COND)
#endif
#if !defined(JXC_ASSERTF)
#define JXC_ASSERTF(...)
#endif
#if !defined(JXC_DEBUG_ASSERT)
#define JXC_DEBUG_ASSERT(...)
#endif
#if !defined(JXC_DEBUG_ASSERTF)
#define JXC_DEBUG_ASSERTF(...)
#endif

#define JXC_MAX_HEREDOC_LENGTH 15

namespace jxc::detail
{
template<typename T, size_t ExpectedSize, size_t ActualSize = sizeof(T)>
struct SizeCheck
{
    static constexpr bool size_matches = ExpectedSize == ActualSize;
    static_assert(size_matches, "Size of type does not match expected value in SizeCheck<Type, ExpectedSize, ActualSize>");
};
} // namespace jxc::detail

#if !defined(JXC_STATIC_ASSERT_SIZEOF)
#define JXC_STATIC_ASSERT_SIZEOF(TYPE, EXPECTED_SIZE) \
    static_assert(::jxc::detail::SizeCheck<TYPE, EXPECTED_SIZE, sizeof(TYPE)>::size_matches, \
        "Size of type " #TYPE " does not match expected size " #EXPECTED_SIZE)
#endif

JXC_BEGIN_NAMESPACE(jxc)

enum class LogLevel : uint8_t
{
    Info = 0,
    Warning,
    Error,
    Fatal,
};

JXC_END_NAMESPACE(jxc)

void _jxc_log_message_string(jxc::LogLevel level, std::string&& msg);
void _jxc_assert_failed_msg(const char* file, int line, const char* cond, std::string&& msg);
void _jxc_assert_failed(const char* file, int line, const char* cond);

JXC_BEGIN_NAMESPACE(jxc)

using LogHandlerFunc = std::function<void(LogLevel, std::string&&)>;
using AssertHandlerFunc = std::function<void(std::string_view file, int line, std::string_view cond, std::string&& msg)>;

const char* log_level_to_string(LogLevel level);

void set_custom_log_handler(const LogHandlerFunc& handler_func);
bool have_custom_log_handler();
inline void clear_custom_log_handler() { set_custom_log_handler(nullptr); }

void set_custom_assert_handler(const AssertHandlerFunc& handler_func);
bool have_custom_assert_handler();
inline void clear_custom_assert_handler() { set_custom_assert_handler(nullptr); }

/// Checks if JXC was compiled with the JXC_ENABLE_JUMP_BLOCK_PROFILER flag
constexpr inline bool is_profiler_enabled() { return JXC_ENABLE_JUMP_BLOCK_PROFILER != 0; }

// invalid size_t constant for use in default arguments
static constexpr size_t invalid_idx = std::numeric_limits<size_t>::max();

JXC_BEGIN_NAMESPACE(detail)

/// Simple pointer-based iterator that can be created from any contigious array
template<typename T, bool IsConst = false>
class PointerIterator
{
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    using reference = std::conditional_t<IsConst, const T&, T&>;

private:
    pointer ptr;

public:
    explicit PointerIterator(pointer ptr) : ptr(ptr) {}
    PointerIterator(const PointerIterator& rhs) = default;
    PointerIterator& operator=(const PointerIterator& rhs) = default;

    reference operator*() const { return *ptr; }
    pointer operator->() { return ptr; }

    PointerIterator& operator++() { ptr++; return *this; }
    PointerIterator operator++() const { PointerIterator iter{ ptr }; ++iter.ptr; return iter; }

    friend bool operator==(const PointerIterator& lhs, const PointerIterator& rhs) { return lhs.ptr == rhs.ptr; }
    friend bool operator!=(const PointerIterator& lhs, const PointerIterator& rhs) { return lhs.ptr != rhs.ptr; }
};

template<typename T>
using ConstPointerIterator = PointerIterator<T, true>;


JXC_FORCEINLINE bool string_view_starts_with(std::string_view view, char ch)
{
#if JXC_CPP20
    return view.starts_with(ch);
#else
    return view.size() > 0 && view.front() == ch;
#endif
}

JXC_FORCEINLINE bool string_view_starts_with(std::string_view view, std::string_view prefix)
{
#if JXC_CPP20
    return view.starts_with(prefix);
#else
    if (view.size() >= prefix.size())
    {
        for (size_t i = 0; i < prefix.size(); i++)
        {
            if (view[i] != prefix[i])
            {
                return false;
            }
        }
        return true;
    }
    return false;
#endif
}

JXC_FORCEINLINE bool string_view_ends_with(std::string_view view, char ch)
{
#if JXC_CPP20
    return view.ends_with(ch);
#else
    return view.size() > 0 && view.back() == ch;
#endif
}

JXC_FORCEINLINE bool string_view_ends_with(std::string_view view, std::string_view suffix)
{
#if JXC_CPP20
    return view.ends_with(suffix);
#else
    if (view.size() >= suffix.size())
    {
        const size_t view_start_idx = view.size() - suffix.size();
        for (size_t i = 0; i < suffix.size(); i++)
        {
            if (view[view_start_idx + i] != suffix[i])
            {
                return false;
            }
        }
        return true;
    }
    return false;
#endif
}

bool find_line_and_col(std::string_view buf, size_t idx, size_t& out_line, size_t& out_col);

JXC_END_NAMESPACE(detail)

struct ErrorInfo
{
    bool is_err = false;
    std::string message;
    size_t buffer_start_idx = invalid_idx;
    size_t buffer_end_idx = invalid_idx;
    size_t line = 0;
    size_t col = 0;

    ErrorInfo() = default;
    ErrorInfo(const ErrorInfo&) = default;
    ErrorInfo& operator=(const ErrorInfo&) = default;
    ErrorInfo(ErrorInfo&&) = default;
    ErrorInfo& operator=(ErrorInfo&&) = default;

    explicit ErrorInfo(const std::string& message, size_t buffer_start_idx = invalid_idx, size_t buffer_end_idx = invalid_idx)
        : is_err(true)
        , message(message)
        , buffer_start_idx(buffer_start_idx)
        , buffer_end_idx(buffer_end_idx)
    {}

    explicit ErrorInfo(std::string&& message, size_t buffer_start_idx = invalid_idx, size_t buffer_end_idx = invalid_idx)
        : is_err(true)
        , message(std::move(message))
        , buffer_start_idx(buffer_start_idx)
        , buffer_end_idx(buffer_end_idx)
    {
    }

    inline explicit operator bool() const { return is_err; }

    // Clears the error state
    void reset();

    // Computes the `line` and `col` members of this ErrorInfo based on the contents of the buffer
    inline bool get_line_and_col_from_buffer(std::string_view buf)
    {
        return detail::find_line_and_col(buf, buffer_start_idx, line, col);
    }

    // Returns a string representing this error. If a buffer is passed in, the error message will contain
    // the part of the buffer that the error is related to.
    std::string to_string(std::string_view buffer = std::string_view{}) const;
};


struct DateTime;


struct Date
{
    int16_t year = 1970;
    int8_t month = 1;
    int8_t day = 1;

    Date() = default;
    Date(int16_t year, int8_t month, int8_t day) : year(year), month(month), day(day) {}
    explicit inline Date(const DateTime& dt);

    inline bool operator==(const Date& rhs) const { return year == rhs.year && month == rhs.month && day == rhs.day; }
    inline bool operator!=(const Date& rhs) const { return !operator==(rhs); }

    inline bool operator==(const DateTime& rhs) const;
    inline bool operator!=(const DateTime& rhs) const { return !operator==(rhs); }
};


struct DateTime
{
    int16_t year = 1970;
    int8_t month = 1;
    int8_t day = 1;
    int8_t hour = 0;
    int8_t minute = 0;
    int8_t second = 0;
    uint32_t nanosecond = 0;
    int8_t tz_hour = 0;
    uint8_t tz_minute : 7;
    uint8_t tz_local : 1;

    DateTime() : tz_minute(0), tz_local(0) {}

    DateTime(
        int16_t year, int8_t month, int8_t day,
        int8_t hour = 0, int8_t minute = 0, int8_t second = 0, uint32_t nanosecond = 0,
        int8_t tz_hour = 0, int8_t tz_minute = 0, bool tz_local_time = false)
        : year(year)
        , month(month)
        , day(day)
        , hour(hour)
        , minute(minute)
        , second(second)
        , nanosecond(nanosecond)
        , tz_hour(tz_hour)
        , tz_minute(tz_minute)
        , tz_local(tz_local_time ? 1 : 0)
    {
    }

    explicit DateTime(const Date& dt,
        int8_t hour = 0, int8_t minute = 0, int8_t second = 0, uint32_t nanosecond = 0,
        int8_t tz_hour = 0, int8_t tz_minute = 0, bool tz_local_time = false)
        : year(dt.year)
        , month(dt.month)
        , day(dt.day)
        , hour(hour)
        , minute(minute)
        , second(second)
        , nanosecond(nanosecond)
        , tz_hour(tz_hour)
        , tz_minute(tz_minute)
        , tz_local(tz_local_time ? 1 : 0)
    {
    }

    static DateTime make_utc(int16_t year, int8_t month, int8_t day,
        int8_t hour = 0, int8_t minute = 0, int8_t second = 0, uint32_t nanosecond = 0)
    {
        return DateTime(year, month, day, hour, minute, second, nanosecond, 0, 0, false);
    }

    static DateTime make_local(int16_t year, int8_t month, int8_t day,
        int8_t hour = 0, int8_t minute = 0, int8_t second = 0, uint32_t nanosecond = 0)
    {
        return DateTime(year, month, day, hour, minute, second, nanosecond, 0, 0, true);
    }

    // Returns true if this DateTime has any time or timezone data.
    // Useful for checking if we can safely serialize as a Date instead of a DateTime.
    inline bool has_time_or_timezone_data() const
    {
        return hour != 0 || minute != 0 || second != 0 || nanosecond != 0 || tz_hour != 0 || tz_minute != 0;
    }

    inline bool is_timezone_local() const { return tz_local == 1; }
    inline bool is_timezone_utc() const { return tz_local == 0 && tz_hour == 0 && tz_minute == 0; }

    inline void set_timezone_utc() { tz_local = 0; tz_hour = 0; tz_minute = 0; }
    inline void set_timezone_local() { tz_local = 1; tz_hour = 0; tz_minute = 0; }
    inline void set_timezone(int8_t new_tz_hour, uint8_t new_tz_minute) { tz_local = 0; tz_hour = new_tz_hour; tz_minute = new_tz_minute; }

    inline bool operator==(const DateTime& rhs) const
    {
        return year == rhs.year && month == rhs.month && day == rhs.day
            && hour == rhs.hour && minute == rhs.minute && second == rhs.second
            && nanosecond == rhs.nanosecond
            && tz_hour == rhs.tz_hour && tz_minute == rhs.tz_minute && tz_local == rhs.tz_local;
    }

    inline bool operator!=(const DateTime& rhs) const
    {
        return !operator==(rhs);
    }

    inline bool operator==(const Date& rhs) const
    {
        return year == rhs.year && month == rhs.month && day == rhs.day;
    }

    inline bool operator!=(const Date& rhs) const
    {
        return !operator==(rhs);
    }
};


inline Date::Date(const DateTime& dt)
    : year(dt.year)
    , month(dt.month)
    , day(dt.day)
{
}


inline bool Date::operator==(const DateTime& rhs) const
{
    return year == rhs.year && month == rhs.month && day == rhs.day;
}


// Date type should be exactly 32 bits
JXC_STATIC_ASSERT_SIZEOF(Date, 4);

JXC_STATIC_ASSERT_SIZEOF(DateTime, 16);

JXC_END_NAMESPACE(jxc)
