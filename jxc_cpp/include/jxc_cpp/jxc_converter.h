#pragma once
#include <sstream>
#include <concepts>
#include <string>
#include <vector>
#include <stdexcept>
#include "jxc/jxc.h"
#include "jxc/jxc_serializer.h"
#include "jxc/jxc_parser.h"


JXC_BEGIN_NAMESPACE(jxc)

class serialize_error : public std::runtime_error
{
    using std::runtime_error::runtime_error; // inherit constructors
};


class parse_error : public std::runtime_error
{
    using std::runtime_error::runtime_error; // inherit constructors
};


JXC_BEGIN_NAMESPACE(detail)


template<typename T, typename Lambda>
std::string init_list_to_string(std::initializer_list<T> values, Lambda&& to_string_callback)
{
    std::ostringstream ss;
    size_t idx = 0;
    for (const T& val : values)
    {
        if (idx > 0)
        {
            ss << ", ";
        }
        ss << to_string_callback(val);
        ++idx;
    }
    return ss.str();
}

template<typename T>
constexpr const char* base_type_annotation()
{
    if constexpr (std::same_as<T, nullptr_t>) { return "nullptr"; }
    else if constexpr (std::same_as<T, bool>) { return "bool"; }
    else if constexpr (std::same_as<T, char>) { return "char"; }
    else if constexpr (std::same_as<T, wchar_t>) { return "wchar"; }
    else if constexpr (std::same_as<T, char8_t>) { return "char8"; }
    else if constexpr (std::same_as<T, char16_t>) { return "char16"; }
    else if constexpr (std::same_as<T, char32_t>) { return "char32"; }
    else if constexpr (std::same_as<T, int8_t>) { return "i8"; }
    else if constexpr (std::same_as<T, int16_t>) { return "i16"; }
    else if constexpr (std::same_as<T, int32_t>) { return "i32"; }
    else if constexpr (std::same_as<T, int64_t>) { return "i64"; }
    else if constexpr (std::same_as<T, uint8_t>) { return "u8"; }
    else if constexpr (std::same_as<T, uint16_t>) { return "u16"; }
    else if constexpr (std::same_as<T, uint32_t>) { return "u32"; }
    else if constexpr (std::same_as<T, uint64_t>) { return "u64"; }
    else if constexpr (std::same_as<T, float>) { return "f32"; }
    else if constexpr (std::same_as<T, double>) { return "f64"; }
    else { return typeid(T).name(); }
}

JXC_END_NAMESPACE(detail)

JXC_BEGIN_NAMESPACE(conv)

template<typename T, typename Lambda>
inline int require(T actual, T expected, const char* type_desc, Lambda&& to_string_callback)
{
    if (actual != expected)
    {
        throw parse_error(jxc::format("Expected {} {}, got {}", type_desc, to_string_callback(expected), to_string_callback(actual)));
    }
    return 0;
}

template<typename T, typename Lambda>
inline int require(T actual, std::initializer_list<T> expected, const char* type_desc, Lambda&& to_string_callback)
{
    int idx = 0;
    for (const T& val : expected)
    {
        if (val == actual)
        {
            return idx;
        }
        ++idx;
    }
    throw parse_error(jxc::format("Unexpected {} {} - expected one of [{}]", type_desc, to_string_callback(actual),
        jxc::detail::init_list_to_string<T, Lambda>(expected, std::forward<Lambda>(to_string_callback))));
}

inline int require(TokenType actual, TokenType expected) { return require(actual, expected, "token", token_type_to_string); }
inline int require(ElementType actual, ElementType expected) { return require(actual, expected, "element", element_type_to_string); }

inline int require(TokenType actual, std::initializer_list<TokenType> expected) { return require(actual, expected, "token", token_type_to_string); }
inline int require(ElementType actual, std::initializer_list<ElementType> expected) { return require(actual, expected, "element", element_type_to_string); }

inline int require(const Token& actual, TokenType expected_type, std::string_view expected_value = {})
{
    require(actual.type, expected_type);
    if (expected_value.size() > 0)
    {
        require<std::string_view>(actual.value.as_view(), expected_value, "identifier", [](auto ident) { return ident; });
    }
    return 0;
}

JXC_END_NAMESPACE(conv)


// Base struct for converter type
template<typename T>
struct Converter
{
    static_assert(std::is_same_v<T, void> && !std::is_same_v<T, void>, "No converter found for type");
    using value_type = T;
    static std::string get_annotation() { return typeid(value_type).name(); }
    static void serialize(Serializer& doc, const value_type& value) {}
    static value_type parse(JumpParser& parser) { return value_type(); }
};


template<>
struct Converter<std::nullptr_t>
{
    using value_type = std::nullptr_t;
    static std::string get_annotation() { return detail::base_type_annotation<value_type>(); }
    static void serialize(Serializer& doc, std::nullptr_t) { doc.value_null(); }
    static value_type parse(JumpParser& parser)
    {
        conv::require(parser.value().token.type, TokenType::Null);
        return nullptr;
    }
};


template<>
struct Converter<bool>
{
    using value_type = bool;
    static std::string get_annotation() { return detail::base_type_annotation<value_type>(); }
    static void serialize(Serializer& doc, bool value) { doc.value_bool(value); }
    static value_type parse(JumpParser& parser)
    {
        //NB. conv::require returns the index of the matched token
        return static_cast<bool>(conv::require(parser.value().token.type, { TokenType::False, TokenType::True }));
    }
};


template<typename T>
concept CharType = std::same_as<T, char> || std::same_as<T, wchar_t> || std::same_as<T, char8_t> || std::same_as<T, char16_t> || std::same_as<T, char32_t>;


template<CharType T>
struct Converter<T>
{
    using value_type = T;
    static std::string get_annotation() { return detail::base_type_annotation<value_type>(); }
    static void serialize(Serializer& doc, value_type value)
    {
        detail::MiniBuffer<char, 8> buf;
        size_t idx = 0;
        const size_t len = jxc::detail::utf8::encode(buf.data(), buf.capacity(), idx, value);
        doc.value_string(buf.as_view(len));
    }
    static value_type parse(JumpParser& parser)
    {
        const Token& tok = parser.value().token;
        ErrorInfo err;
        switch (tok.type)
        {
        case TokenType::String:
        {
            // parse as a string, erroring out if the string is not exactly one (ascii) character
            size_t req_buffer_size = 0;
            if (!util::get_string_token_required_buffer_size(tok, req_buffer_size, err))
            {
                throw parse_error(jxc::format("Failed parsing string: {}", err.to_string()));
            }
            else if (req_buffer_size == 0)
            {
                throw parse_error(jxc::format("Expected {}, got empty string", get_annotation()));
            }
            else if (req_buffer_size >= 8)
            {
                // req_buffer_size is not the actual string length, but we can still improve perf slightly here
                // by only parsing the entire string if we know it's very short.
                throw parse_error(jxc::format("Expected {}, got string with more than one character", get_annotation()));
            }
            detail::MiniBuffer<char, 8> buf;
            size_t buf_len = 0;
            if (!util::parse_string_token_to_buffer(tok, buf.data(), buf.capacity(), buf_len, err))
            {
                throw parse_error(jxc::format("Failed parsing string: {}", err.to_string()));
            }

            if constexpr (sizeof(value_type) == sizeof(char))
            {
                if (buf_len != 1)
                {
                    throw parse_error(jxc::format("Expected {}, got string with length {}", get_annotation(), buf_len));
                }
                return static_cast<value_type>(buf[0]);
            }
            else
            {
                size_t buf_idx = 0;
                JXC_ASSERT(buf_len <= buf.capacity());
                const uint32_t codepoint = detail::utf8::decode(buf.data(), buf_len, buf_idx);
                if (codepoint <= std::numeric_limits<value_type>::max())
                {
                    return static_cast<value_type>(codepoint);
                }
                else
                {
                    throw parse_error(jxc::format("Codepoint {} too large for char type {}", codepoint, get_annotation()));
                }
            }
        }
        case TokenType::ByteString:
        {
            // parse as a byte string, erroring out if it's not exactly one byte
            const size_t req_buffer_size = util::get_byte_buffer_required_size(tok.value.data(), tok.value.size());
            if (req_buffer_size >= 8)
            {
                // don't bother calling parse_bytes_token if the required buffer size is too big
                throw parse_error(jxc::format("Byte string too large to contain exactly one {}", get_annotation()));
            }
            detail::MiniBuffer<uint8_t, 8> buf;
            size_t num_bytes_written = 0;
            if (!util::parse_bytes_token(tok, buf.data(), buf.capacity(), num_bytes_written, err))
            {
                throw parse_error(jxc::format("Failed to parse byte string: {}", err.to_string()));
            }

            if constexpr (sizeof(value_type) == sizeof(char))
            {
                if (num_bytes_written != 1)
                {
                    throw parse_error(jxc::format("Expected byte string with exactly one {}, got {}", get_annotation(), num_bytes_written));
                }
                return static_cast<value_type>(buf[0]);
            }
            else
            {
                size_t buf_idx = 0;
                const uint32_t codepoint = detail::utf8::decode(reinterpret_cast<char*>(buf.data()), num_bytes_written, buf_idx);
                if (codepoint <= std::numeric_limits<value_type>::max())
                {
                    return static_cast<value_type>(codepoint);
                }
                else
                {
                    throw parse_error(jxc::format("Codepoint {} too large for char type {}", codepoint, get_annotation()));
                }
            }
        }
        case TokenType::Number:
        {
            // parse as an integer, erroring out if the integer is not valid for int8_t or uint8_t
            int64_t result = 0;
            std::string err_msg;
            if (!util::parse_number_simple(tok.value.as_view(), result, &err_msg))
            {
                throw parse_error(jxc::format("Failed to parse number: {}", err_msg));
            }

            // char is sometimes signed in weird situations.
            // If the number is negative, use c-style behavior where possible: treat it as int8_t and then c-style cast it to char.
            if (result < 0 && result >= std::numeric_limits<int8_t>::min())
            {
                char result_char = (char)static_cast<int8_t>(result);
                return static_cast<value_type>(result_char);
            }
            else if (result >= 0 && result <= std::numeric_limits<value_type>::max())
            {
                return static_cast<value_type>(result);
            }
            else
            {
                throw parse_error(jxc::format("Failed to parse {}: integer value {} is not in range {}..{}", get_annotation(), result,
                    std::numeric_limits<int8_t>::min(), std::numeric_limits<value_type>::max()));
            }
        }
        case TokenType::Null:
        {
            return static_cast<char>(0);
        }
        default:
            conv::require(tok.type, { TokenType::String, TokenType::ByteString, TokenType::Number, TokenType::Null });
            break;
        }
        return '\0';
    }
};


template<std::unsigned_integral T>
struct Converter<T>
{
    using value_type = T;
    static std::string get_annotation() { return detail::base_type_annotation<value_type>(); }
    static void serialize(Serializer& doc, value_type value) { doc.value_uint(static_cast<int64_t>(value)); }
    static value_type parse(JumpParser& parser)
    {
        conv::require(parser.value().token.type, TokenType::Number);
        value_type result = 0;
        std::string err;
        if (!util::parse_number_simple<value_type>(parser.value().token.value.as_view(), result, &err))
        {
            throw parse_error(jxc::format("Failed to parse unsigned integer: {}", err));
        }
        return result;
    }
};


template<std::signed_integral T>
struct Converter<T>
{
    using value_type = T;
    static std::string get_annotation() { return detail::base_type_annotation<value_type>(); }
    static void serialize(Serializer& doc, value_type value) { doc.value_int(static_cast<int64_t>(value)); }
    static value_type parse(JumpParser& parser)
    {
        conv::require(parser.value().token.type, TokenType::Number);
        value_type result = 0;
        std::string err;
        if (!util::parse_number_simple<value_type>(parser.value().token.value.as_view(), result, &err))
        {
            throw parse_error(jxc::format("Failed to parse signed integer: {}", err));
        }
        return result;
    }
};


template<std::floating_point T>
struct Converter<T>
{
    using value_type = T;
    static std::string get_annotation() { return detail::base_type_annotation<value_type>(); }
    static void serialize(Serializer& doc, value_type value) { doc.value_float(static_cast<double>(value)); }
    static value_type parse(JumpParser& parser)
    {
        conv::require(parser.value().token.type, TokenType::Number);
        value_type result = 0;
        std::string err;
        if (!util::parse_number_simple<value_type>(parser.value().token.value.as_view(), result, &err))
        {
            throw parse_error(jxc::format("Failed to parse floating point number: {}", err));
        }
        return result;
    }
};


template<>
struct Converter<std::string>
{
    using value_type = std::string;
    static std::string get_annotation() { return "string"; }
    static void serialize(Serializer& doc, const value_type& value) { doc.value_string(value); }
    static value_type parse(JumpParser& parser)
    {
        value_type result;
        conv::require(parser.value().token.type, TokenType::String);
        ErrorInfo err;
        if (!util::parse_string_token(parser.value().token, result, err))
        {
            throw parse_error(jxc::format("Failed to parse string: {}", err.to_string()));
        }
        return result;
    }
};


template<>
struct Converter<std::vector<uint8_t>>
{
    using value_type = std::vector<uint8_t>;
    static std::string get_annotation() { return "bytes"; }
    static void serialize(Serializer& doc, const value_type& value) { doc.value_bytes(value.data(), value.size()); }
    static value_type parse(JumpParser& parser)
    {
        value_type result;
        conv::require(parser.value().token.type, TokenType::ByteString);
        ErrorInfo err;
        if (!util::parse_bytes_token(parser.value().token, result, err))
        {
            throw parse_error(jxc::format("Failed to parse bytes: {}", err.to_string()));
        }
        return result;
    }
};


JXC_BEGIN_NAMESPACE(conv)


template<typename T>
std::string serialize(const T& value, const jxc::SerializerSettings& settings = jxc::SerializerSettings())
{
    jxc::StringOutputBuffer buffer;
    jxc::Serializer doc(&buffer, settings);
    jxc::Converter<T>().serialize(doc, value);
    doc.flush();
    return buffer.to_string();
}

template<typename T>
T parse(const std::string& buffer)
{
    jxc::JumpParser parser(buffer);
    if (!parser.next())
    {
        if (parser.has_error())
        {
            jxc::ErrorInfo err = parser.get_error();
            err.get_line_and_col_from_buffer(buffer);
            throw parse_error(jxc::format("Parse error: {}", err.to_string(buffer)));
        }
        else
        {
            throw parse_error("Unexpected end of stream");
        }
    }

    return Converter<T>::parse(parser);
}

template<typename T>
std::optional<T> try_parse(const std::string& buffer, std::string* out_error_message = nullptr)
{
    try
    {
        return parse<T>(buffer);
    }
    catch (const parse_error& err)
    {
        if (out_error_message != nullptr)
        {
            *out_error_message = err.what();
        }
    }
    return std::nullopt;
}

JXC_END_NAMESPACE(conv)

JXC_END_NAMESPACE(conv)
