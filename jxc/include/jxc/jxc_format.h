/**
 * jxc_format
 * Author: Judd Cohen
 * Copyright: 2023
 * License: MIT
 * 
 * jxc_format is a simple, standalone string formatting library that has an API similar to libfmt or std::format (in C++20).
 * Libfmt is excellent but a bit too heavy of a dependency, and std::format just isn't ready yet.
 * 
 * The intent is to eventually switch to std::format once it's broadly supported, but that may be a while, and the core JXC
 * library is still targeting C++17.
 * 
 * jxc_format has an extremely small feature set - basically just the bare minimum required for what JXC needs, which is
 * essentially just error message and debug representation formatting. This is also why performance is not a large concern -
 * this library is not intended to be used in any hot codepaths.
 * 
 * The behavior of this library is intended to be as close as possible to libfmt and std::format, to make migration as
 * easy as possible.
 */
#pragma once
#include <string>
#include <string_view>
#include <limits>
#include <charconv>
#include <sstream>
#include <type_traits>
#include <stdexcept>
#include <cstdio> // for std::FILE*
#include <bitset> // for binary integer formatting

//NB. This header is expected to be standalone and should not include any jxc headers

// Can't rely on JXC's normal error handling here because that error handling all uses this string format function.
// You can define this macro yourself if you want to use a different error mechanism (eg. if you don't want any exceptions).
#ifndef JXC_FORMATTING_ERROR
#define JXC_FORMATTING_ERROR(...) throw std::runtime_error(::jxc::detail_format::concat_args(__VA_ARGS__))
#endif


namespace jxc
{

// format type for floats with dynamic precision
template<typename T>
struct FloatPrecision
{
    static_assert(std::is_floating_point_v<T>, "FloatPrecision requires a floating point type");
    T value = static_cast<T>(0);
    int precision = 0;
    constexpr FloatPrecision(T value, int precision) : value(value), precision(precision) {}
};


namespace detail_format
{

// Function that simply concats all arguments and returns a string.
// Useful for formatting errors that occur in this library.
template<typename... TArgs>
inline std::string concat_args(TArgs&&... args)
{
    std::ostringstream ss;
    (ss << ... << args);
    return ss.str();
}

class FormatParser
{
public:
    static constexpr size_t invalid_format_arg = std::numeric_limits<size_t>::max();

private:
    std::string_view format_string;
    size_t value_start = 0;
    size_t value_size = 0;
    bool inside_argument = false;

public:
    FormatParser(std::string_view format_string)
        : format_string(format_string)
    {
    }

    size_t num_arguments() const
    {
        size_t num_args = 0;
        size_t idx = 0;
        const size_t len = format_string.size();
        while (idx < len)
        {
            if (format_string[idx] == '{')
            {
                if (idx < len - 1 && format_string[idx + 1] == '{')
                {
                    // double '{' means don't start a new argument
                    idx += 2;
                }
                else
                {
                    // single '{' means start an argument
                    ++num_args;

                    // skip to the end of the argument
                    while (idx < len && format_string[idx] != '}')
                    {
                        ++idx;
                    }

                    // if we didn't end on a '}' character then this is a syntax error
                    if (format_string[idx] != '}')
                    {
                        return invalid_format_arg;
                    }
                }
            }
            else
            {
                ++idx;
            }
        }
        return num_args;
    }

    std::string_view value() const
    {
        if (value_start == invalid_format_arg)
        {
            return std::string_view{ nullptr, 0 };
        }
        else if (inside_argument)
        {
#if JXC_DEBUG
            // must be at least "{}"
            if (value_size < 2)
            {
                JXC_FORMATTING_ERROR("Format argument size too small");
            }
#endif
            // strip braces from the return value
            return (value_size > 2)
                ? format_string.substr(value_start + 1, value_size - 2)
                : std::string_view("");
        }

        return format_string.substr(value_start, value_size);
    }

    bool value_is_argument() const
    {
        return inside_argument && value_start != invalid_format_arg && value_size > 0;
    }

    bool next()
    {
        const size_t format_string_size = format_string.size();
        const size_t new_start = value_start + value_size;
        size_t new_end = new_start;

        if (new_start >= format_string_size)
        {
            return false;
        }
        else if (new_start == format_string_size - 1)
        {
            // last character, so this can't be the start of a new argument
            value_start = new_start;
            value_size = 1;
            inside_argument = false;
            return true;
        }

        inside_argument = false;
        if (format_string[new_end] == '{')
        {
            if (format_string[new_end + 1] == '{')
            {
                // return literal '{' char
                value_start = new_start + 1; // skip over first '{' char (it's effectively just an escape char)
                value_size = 1;
                return true;
            }
            else
            {
                // skip to the end of the argument
                while (new_end < format_string_size && format_string[new_end] != '}')
                {
                    ++new_end;
                }

                if (format_string[new_end] == '}')
                {
                    // format argument slice
                    inside_argument = true;
                    value_start = new_start;
                    value_size = (new_end - new_start) + 1;
                    return true;
                }
                else
                {
                    // error - format argument is not terminated with a '}'
                    value_start = invalid_format_arg;
                    value_size = 0;
                    return false;
                }
            }
        }
        else
        {
            // non-argument slice
            while (new_end < format_string_size && format_string[new_end] != '{')
            {
                ++new_end;
            }
            value_start = new_start;
            value_size = new_end - new_start;
            return true;
        }
    }
};


class FormatBuffer
{
private:
    std::ostringstream buffer;

public:
    void reset_precision()
    {
        buffer.precision(12);
        buffer << std::defaultfloat;
    }

    void set_fixed(int new_precision)
    {
        buffer.precision(new_precision);
        buffer << std::fixed;
    }

    void setup_next_value()
    {
        reset_precision(); // reset float
        buffer << std::dec; // reset int
    }

    template<typename T>
    inline FormatBuffer& operator<<(const T& value)
    {
        buffer << value;
        return *this;
    }

    auto precision(int new_precision)
    {
        return buffer.precision(new_precision);
    }

    inline std::string to_string() const
    {
        return buffer.str();
    }
};


template<typename TTuple, typename Lambda, size_t... IdxSeq>
constexpr void iter_tuple_impl(TTuple&& tup, Lambda&& func, std::index_sequence<IdxSeq...>)
{
    ( func(std::integral_constant<size_t, IdxSeq>{}, std::get<IdxSeq>(tup)),... );
}


template<typename... T, typename Lambda>
constexpr void iter_tuple(std::tuple<T...>& tup, Lambda&& func)
{
    iter_tuple_impl(tup, std::forward<Lambda>(func), std::make_index_sequence<sizeof...(T)>{});
}


template<typename T>
void parse_float_format(FormatBuffer& output, std::string_view arg, T value)
{
    static_assert(std::is_floating_point_v<T>, "parse_float_format requires a float type");
    if (arg.size() > 0)
    {
        // Currently only supports the syntax `:.4f`, where `4` indicates the number of digits
        // using fixed-width precision.
        if (!(arg.size() >= 4 && arg[0] == ':' && arg[1] == '.' && arg.back() == 'f'))
        {
            JXC_FORMATTING_ERROR("Invalid float format `", arg, "`");
        }
        std::string_view float_prec_fmt = arg.substr(2, arg.size() - 3);
        int float_prec = 0;
        auto err = std::from_chars(float_prec_fmt.data(), float_prec_fmt.data() + float_prec_fmt.size(), float_prec);
        if (err.ec != std::errc(0))
        {
            JXC_FORMATTING_ERROR("Failed to parse floating point precision value '", float_prec_fmt, "'");
        }
        output.set_fixed(float_prec);
    }
    output << value;
}


template<typename T>
void parse_int_format(FormatBuffer& output, std::string_view arg, T value)
{
    static_assert(std::is_integral_v<T>, "parse_int_format requires an int type");
    if (arg.size() == 0)
    {
        output << value;
        return;
    }

    if (arg[0] != ':' || arg.size() != 2)
    {
        JXC_FORMATTING_ERROR("Invalid format string for int: `", arg, "`");
    }

    const char int_type = arg[1];
    switch (int_type)
    {
    case 'x':
        output << std::hex << value;
        break;
    case 'o':
        output << std::oct << value;
        break;
    case 'b':
    {
        if (value == 0)
        {
            output << '0';
        }
        else
        {
            const std::string result = std::bitset<sizeof(T) * 8>(value).to_string();
            size_t first_non_zero_bit = std::numeric_limits<size_t>::max();
            for (size_t i = 0; i < result.size(); i++)
            {
                if (result[i] != '0')
                {
                    first_non_zero_bit = i;
                    break;
                }
            }

            if (first_non_zero_bit < result.size())
            {
                output << result.substr(first_non_zero_bit);
            }
            else
            {
                output << result;
            }
        }
        break;
    }
    default:
        JXC_FORMATTING_ERROR("Invalid int format char '", int_type, "'");
        break;
    }
}


template<typename T>
struct Formatter
{
    void format(FormatBuffer& output, std::string_view arg, const T& value) const
    {
        if (arg.size() != 0)
        {
            JXC_FORMATTING_ERROR("Generic formatter has no valid format arguments (got '", arg, "')");
        }
        output << value;
    }
};


#define JXC_DEFINE_FLOAT_FORMATTER(TYPE) \
template<> \
struct Formatter<TYPE> { \
    void format(FormatBuffer& output, std::string_view arg, TYPE value) const { \
        parse_float_format(output, arg, value); \
    } \
}

JXC_DEFINE_FLOAT_FORMATTER(float);
JXC_DEFINE_FLOAT_FORMATTER(double);

#undef JXC_DEFINE_FLOAT_FORMATTER

#define JXC_DEFINE_INT_FORMATTER(TYPE) \
template<> \
struct Formatter<TYPE> { \
    void format(FormatBuffer& output, std::string_view arg, TYPE value) const { \
        parse_int_format<TYPE>(output, arg, value); \
    } \
}

JXC_DEFINE_INT_FORMATTER(uint8_t);
JXC_DEFINE_INT_FORMATTER(uint16_t);
JXC_DEFINE_INT_FORMATTER(uint32_t);
JXC_DEFINE_INT_FORMATTER(uint64_t);
JXC_DEFINE_INT_FORMATTER(int8_t);
JXC_DEFINE_INT_FORMATTER(int16_t);
JXC_DEFINE_INT_FORMATTER(int32_t);
JXC_DEFINE_INT_FORMATTER(int64_t);

#undef JXC_DEFINE_INT_FORMATTER


template<>
struct Formatter<bool>
{
    void format(FormatBuffer& output, std::string_view arg, bool value) const
    {
        if (arg.size() != 0)
        {
            JXC_FORMATTING_ERROR("bool formatter has no valid format arguments (got '", arg, "')");
        }
        output << (value ? "true" : "false");
    }
};


template<>
struct Formatter<nullptr_t>
{
    void format(FormatBuffer& output, std::string_view arg, nullptr_t) const
    {
        if (arg.size() != 0)
        {
            JXC_FORMATTING_ERROR("nullptr_t formatter has no valid format arguments (got '", arg, "')");
        }
        output << "0x0";
    }
};


// formatter for floats with dynamic precision
template<typename T>
struct Formatter<FloatPrecision<T>>
{
    void format(FormatBuffer& output, std::string_view arg, const FloatPrecision<T>& value) const
    {
        if (arg.size() != 0)
        {
            JXC_FORMATTING_ERROR("FloatPrecision formatter has no valid format arguments (got '", arg, "')");
        }
        output.set_fixed(value.precision);
        output << value.value;
    }
};


template<typename T>
using FormatArgType = std::remove_cv_t<std::remove_reference_t<T>>;


template<typename T>
auto transform_format_argument(T&& arg)
{
    using arg_t = detail_format::FormatArgType<decltype(arg)>;
    if constexpr (std::is_same_v<arg_t, uint8_t> || std::is_same_v<arg_t, int8_t>)
    {
        return static_cast<int32_t>(arg);
    }
    else
    {
        return std::forward<T>(arg);
    }
}


// convert string literal format arguments to 
template<size_t N>
auto transform_format_argument(const char (&string_literal)[N])
{
    static_assert(N > 0, "N must be greater than 0 because str[N] should be the null terminator");
    return std::string_view(string_literal, N - 1);
}

} // namespace detail_format


template<size_t N>
std::string format(const char (&format_str)[N])
{
    return std::string(format_str);
}


template<typename... TArgs>
std::string format(std::string_view format_str, TArgs&&... args)
{
    auto values = std::make_tuple(detail_format::transform_format_argument(std::forward<TArgs>(args))...);
    constexpr size_t num_values = std::tuple_size_v<decltype(values)>;
    detail_format::FormatParser parser(format_str);
    const size_t num_args_in_format_str = parser.num_arguments();
    if (num_args_in_format_str == detail_format::FormatParser::invalid_format_arg)
    {
        JXC_FORMATTING_ERROR("Syntax error in format string `", format_str, "`");
    }
    else if (num_args_in_format_str != num_values)
    {
        JXC_FORMATTING_ERROR("Format string `", format_str, "` has ", num_args_in_format_str,
            " arguments, but format function got ", num_values, " arguments");
    }

    detail_format::FormatBuffer buffer;

    // format each format argument's value into the buffer
    detail_format::iter_tuple(values, [&parser, &buffer](size_t /*i*/, auto&& arg)
    {
        using arg_t = detail_format::FormatArgType<decltype(arg)>;
        while (parser.next())
        {
            if (parser.value_is_argument())
            {
                buffer.setup_next_value();
                detail_format::Formatter<arg_t>().format(buffer, parser.value(), std::forward<decltype(arg)>(arg));
                return;
            }
            else
            {
                buffer << parser.value();
            }
        }
    });

    // clear out remaining non-format-argument string data from the parser
    while (parser.next())
    {
        if (parser.value_is_argument())
        {
            JXC_FORMATTING_ERROR("Unexpected extra arguments in format string '", format_str, "'");
        }
        buffer << parser.value();
    }

    return buffer.to_string();
}

template<typename S, typename... TArgs>
void print(std::FILE* dest, const S& format_str, TArgs&&... args)
{
    const auto str = jxc::format(format_str, std::forward<TArgs>(args)...);
    std::fwrite(str.data(), sizeof(char), str.size(), dest);
}

template<typename S, typename... TArgs>
void print(const S& format_str, TArgs&&... args)
{
    print(stdout, format_str, std::forward<TArgs>(args)...);
}

} // namespace jxc
