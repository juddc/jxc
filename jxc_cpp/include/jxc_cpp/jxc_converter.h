#pragma once
#include <sstream>
#include <concepts>
#include <string>
#include <vector>
#include <stdexcept>
#include "jxc/jxc.h"
#include "jxc/jxc_serializer.h"
#include "jxc/jxc_parser.h"
#include "jxc_cpp/jxc_cpp_core.h"


JXC_BEGIN_NAMESPACE(jxc)


JXC_BEGIN_NAMESPACE(conv)
class Parser;
JXC_END_NAMESPACE(conv)


// Base struct for auto converters
template<typename T>
struct Converter
{
    // This is a strange-looking static assert, but it has the effect of getting the type without a converter into the error message
    static_assert(std::is_same_v<T, void> && !std::is_same_v<T, void>, "No converter found for type");

    using value_type = T;

    static std::string get_annotation()
    {
        return typeid(value_type).name();
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        return value_type();
    }
};


template<typename T>
concept ConverterWithSerialize = requires { typename Converter<T>::value_type; }
    && requires
    {
        { Converter<T>::get_annotation() } -> std::convertible_to<std::string>;
    }
    && requires(Serializer& doc, typename Converter<T>::value_type& value)
    {
        { Converter<T>::serialize(doc, value) };
    };


template<typename T>
concept ConverterWithParse = requires { typename Converter<T>::value_type; }
    && requires
    {
        { Converter<T>::get_annotation() } -> std::convertible_to<std::string>;
    }
    && requires(conv::Parser& p, TokenSpan anno)
    {
        { Converter<T>::parse(p, anno) } -> std::same_as<typename Converter<T>::value_type>;
    };


template<typename T>
concept HasConverter = ConverterWithParse<T> && ConverterWithSerialize<T>;


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

// Converts a C++ type signature to a JXC annotation. Intended for use in macros.
std::string cpp_type_to_annotation(std::string_view cpp_type_src);

template<typename T>
std::string base_type_annotation()
{
#define JXC_TYPE_TO_STR(TYPE) constexpr (std::same_as<T, TYPE>) { return #TYPE; }
    if constexpr (std::is_same_v<T, std::nullptr_t>) { return "nullptr_t"; }
    else if JXC_TYPE_TO_STR(bool)
    else if JXC_TYPE_TO_STR(char)
    else if JXC_TYPE_TO_STR(wchar_t)
    else if JXC_TYPE_TO_STR(char8_t)
    else if JXC_TYPE_TO_STR(char16_t)
    else if JXC_TYPE_TO_STR(char32_t)
    else if JXC_TYPE_TO_STR(int8_t)
    else if JXC_TYPE_TO_STR(int16_t)
    else if JXC_TYPE_TO_STR(int32_t)
    else if JXC_TYPE_TO_STR(int64_t)
    else if JXC_TYPE_TO_STR(uint8_t)
    else if JXC_TYPE_TO_STR(uint16_t)
    else if JXC_TYPE_TO_STR(uint32_t)
    else if JXC_TYPE_TO_STR(uint64_t)
    else if JXC_TYPE_TO_STR(float)
    else if JXC_TYPE_TO_STR(double)
    else if constexpr (std::same_as<T, const char*> || std::same_as<T, std::string> || std::same_as<T, std::string_view>
        || std::same_as<T, FlexString>)
    {
        return "string";
    }
    else if constexpr (std::same_as<T, const wchar_t*> || std::same_as<T, std::wstring> || std::same_as<T, std::wstring_view>)
    {
        return "wstring";
    }
    else if constexpr (std::same_as<T, const char8_t*> || std::same_as<T, std::u8string> || std::same_as<T, std::u8string_view>)
    {
        return "u8string";
    }
    else if constexpr (std::same_as<T, const char16_t*> || std::same_as<T, std::u16string> || std::same_as<T, std::u16string_view>)
    {
        return "u16string";
    }
    else if constexpr (std::same_as<T, const char32_t*> || std::same_as<T, std::u32string> || std::same_as<T, std::u32string_view>)
    {
        return "u32string";
    }
    else
    {
        const std::string cpp_type_name = detail::get_type_name<T>();
        return detail::cpp_type_to_annotation(cpp_type_name);
    }
#undef JXC_TYPE_TO_STR
}

JXC_END_NAMESPACE(detail)


JXC_BEGIN_NAMESPACE(conv)


class Parser : public JumpParser
{
private:
    std::string source;

public:
    Parser(std::string_view jxc_source)
        : JumpParser()
        , source(std::string(jxc_source))
    {
        // Now that we've copied jxc_source to an internal buffer, init the JumpParser using that buffer,
        // so that we own the memory for the JXC buffer.
        reset(source);
    }

    template<typename T>
    inline T parse_value(TokenSpan generic_annotation = TokenSpan())
    {
        return Converter<T>::parse(*this, generic_annotation);
    }

    // advances the parser, throwing a parse_error if there are no elements remaining
    inline void require_next()
    {
        if (!next())
        {
            throw parse_error("Unexpected end of stream");
        }
    }

    template<typename T, typename Lambda>
    inline int require(T actual, T expected, const char* type_desc, Lambda&& to_string_callback) const
    {
        if (actual != expected)
        {
            throw parse_error(jxc::format("Expected {} {}, got {}", type_desc, to_string_callback(expected), to_string_callback(actual)));
        }
        return 0;
    }

    template<typename T, typename Lambda>
    inline int require(T actual, std::initializer_list<T> expected, const char* type_desc, Lambda&& to_string_callback) const
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
            detail::init_list_to_string<T, Lambda>(expected, std::forward<Lambda>(to_string_callback))));
    }

    // throws parse_error if the specified token type does not match the expected value
    inline int require(TokenType actual, TokenType expected) const { return require(actual, expected, "token", token_type_to_string); }

    // throws parse_error if the current token type does not match the expected value
    inline int require(TokenType expected) const { return require(value().token.type, expected, "token", token_type_to_string); }

    // throws parse_error if the specified element type does not match the expected value
    inline int require(ElementType actual, ElementType expected) const { return require(actual, expected, "element", element_type_to_string); }

    // throws parse_error if the current element type does not match the expected value
    inline int require(ElementType expected) const { return require(value().type, expected, "element", element_type_to_string); }

    // throws parse_error if the specified token type is not one of the expected values
    inline int require(TokenType actual, std::initializer_list<TokenType> expected) { return require(actual, expected, "token", token_type_to_string); }

    // throws parse_error if the current token type is not one of the expected values
    inline int require(std::initializer_list<TokenType> expected) { return require(value().token.type, expected, "token", token_type_to_string); }

    // throws parse_error if the specified element type is not one of the expected values
    inline int require(ElementType actual, std::initializer_list<ElementType> expected) { return require(actual, expected, "element", element_type_to_string); }

    // throws parse_error if the current element type is not one of the expected values
    inline int require(std::initializer_list<ElementType> expected) { return require(value().type, expected, "element", element_type_to_string); }

    // throws parse_error if the specified token is not an identifier with the expected value
    inline int require_identifier_token(const Token& actual, std::string_view identifier_value) const
    {
        require(actual.type, TokenType::Identifier);
        require<std::string_view>(actual.value.as_view(), identifier_value, "identifier", [](const auto& ident) { return ident; });
        return 0;
    }

    // throws parse_error if the current token is not an identifier with the expected value
    inline int require_identifier_token(std::string_view identifier_value) const
    {
        require(TokenType::Identifier);
        require<std::string_view>(value().token.value.as_view(), identifier_value, "identifier", [](const auto& ident) { return ident; });
        return 0;
    }

    // throws parse_error if the specified annotation is not one of the expected values.
    // If annotation_optional is false, then parse_error will be thrown if no annotation was specified.
    // NB. This function is intended for simple cases, and will only match single-identifier annotations
    std::string_view require_identifier_annotation(TokenSpan anno, std::initializer_list<std::string_view> expected_identifiers, bool annotation_optional = false) const;

    // throws parse_error if the annotation for the current value is not one of the expected values.
    // If annotation_optional is false, then parse_error will be thrown if no annotation was specified.
    // NB. This function is intended for simple cases, and will only match single-identifier annotations
    inline std::string_view require_identifier_annotation(std::initializer_list<std::string_view> expected_identifiers, bool annotation_optional = false) const
    {
        return require_identifier_annotation(value().annotation, expected_identifiers, annotation_optional);
    }

    // Throws parse_error if the current value has any annotation at all
    inline void require_no_annotation() const
    {
        TokenSpan anno = value().annotation;
        if (anno.size() > 0)
        {
            throw parse_error(jxc::format("Unexpected annotation {} (no annotation is valid for this value)", jxc::detail::debug_string_repr(anno.source())), value());
        }
    }

    // returns an AnnotationParser for the specified annotation
    inline AnnotationParser parse_annotation(TokenSpan anno) const
    {
        return AnnotationParser(anno, [](const ErrorInfo& err)
        {
            throw parse_error("Failed parsing annotation", err);
        });
    }

    // returns an AnnotationParser for the current element
    inline AnnotationParser parse_annotation() const
    {
        return parse_annotation(value().annotation);
    }

    // If the current value has its own annotation, returns that.
    // Otherwise returns the supplied generic annotation from the value's parent.
    inline TokenSpan get_value_annotation(TokenSpan generic_anno) const
    {
        if (value().annotation)
        {
            return value().annotation;
        }
        return generic_anno;
    }

    // If the token is a string, parses and returns the string.
    // if the token is an identifier, returns the identifier as a string.
    // Otherwise, throws parse_error.
    std::string parse_token_as_string(const Token& token);

    // Parses an object key token as a string (string or identifier types), number (integer type only), or bool.
    template<typename T>
    T parse_token_as_object_key(const Token& token)
    {
        static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, bool> || std::is_integral_v<T>,
            "parse_object_key_token only accepts std::string, bool, and integer types");

        if constexpr (std::is_same_v<T, std::string>)
        {
            std::string result;
            switch (token.type)
            {
            case TokenType::String:
                // if the token is a string, parse it first (this handles string escapes, raw strings, heredocs, etc.)
                if (!util::parse_string_token<std::string>(token, result, error))
                {
                    throw parse_error("Failed to parse string object key", error);
                }
                break;

            case TokenType::Identifier:
                // if the token is an identifier, we can just return the value - no need to parse it
                result = std::string(token.value.as_view());
                break;

            default:
                throw parse_error(jxc::format("Expected string or identifier, got {}", token_type_to_string(token.type)), token);
            }

            return result;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (token.type == TokenType::False)
            {
                return false;
            }
            else if (token.type == TokenType::True)
            {
                return true;
            }
            else
            {
                throw parse_error(jxc::format("Expected true or false, got {}", token_type_to_string(token.type)), token);
            }
        }
        else if constexpr (std::is_integral_v<T>)
        {
            T result = static_cast<T>(0);
            if (!util::parse_number_simple<T>(token, result, error))
            {
                throw parse_error("Failed to parse numeric object key", token);
            }
            return result;
        }
        else
        {
            []<bool Err = true>() { static_assert(!Err, "Invalid type for parse_token_as_object_key()"); };
            return T();
        }
    }

    // If the token is a number, parse and return it.
    // Throws parse_error on failure.
    template<typename T>
    T parse_token_as_number(const Token& token)
    {
        if (token.type != TokenType::Number)
        {
            throw parse_error(jxc::format("Expected Number token, got {}", token_type_to_string(token.type)), token);
        }

        T result = static_cast<T>(0);
        std::string suffix;
        if (!util::parse_number_simple<T>(token, result, error, &suffix))
        {
            throw parse_error("Failed parsing number", error);
        }

        // don't allow any numeric suffix
        if (suffix.size() > 0)
        {
            throw parse_error(jxc::format("Unexpected numeric suffix {}", detail::debug_string_repr(suffix)), token);
        }

        return result;
    }

    // If the token is a number, parse and return it.
    // Accepts a single allowed suffix. Throws a parse_error if the number does not have that suffix.
    // Throws parse_error on failure.
    template<typename T>
    T parse_token_as_number(const Token& token, std::string_view allowed_suffix)
    {
        if (token.type != TokenType::Number)
        {
            throw parse_error(jxc::format("Expected Number token, got {}", token_type_to_string(token.type)), token);
        }

        T result = static_cast<T>(0);
        std::string suffix;
        if (!util::parse_number_simple<T>(token, result, error, &suffix))
        {
            throw parse_error("Failed parsing number", error);
        }

        // make sure we have the required suffix
        if (suffix != allowed_suffix)
        {
            throw parse_error(jxc::format("Expected numeric suffix {}, got {}",
                detail::debug_string_repr(allowed_suffix), detail::debug_string_repr(suffix)), token);
        }

        return result;
    }

    // If the token is a number, parse and return it.
    // Accepts a list of allowed suffixes. If the number's suffix is not in the list, throws parse_error.
    // Returns the number's suffix in out_suffix.
    // Throws parse_error on failure.
    template<typename T>
    T parse_token_as_number(const Token& token, std::initializer_list<std::string_view> allowed_suffixes, std::string& out_suffix)
    {
        if (token.type != TokenType::Number)
        {
            throw parse_error(jxc::format("Expected Number token, got {}", token_type_to_string(token.type)), token);
        }

        // split the number value into component parts - we want to validate the suffix first
        util::NumberTokenSplitResult number_parts;
        if (!util::split_number_token_value(token, number_parts, error))
        {
            throw parse_error("Failed to parse number", error);
        }

        bool suffix_is_valid = false;
        for (const auto& suffix : allowed_suffixes)
        {
            if (number_parts.suffix == suffix)
            {
                suffix_is_valid = true;
                break;
            }
        }

        if (suffix_is_valid)
        {
            out_suffix = std::string(number_parts.suffix);
        }
        else
        {
            std::ostringstream ss;
            ss << "Invalid numeric suffix " << detail::debug_string_repr(number_parts.suffix);
            ss << " (allowed values: ";
            for (const auto& suffix : allowed_suffixes)
            {
                ss << detail::debug_string_repr(suffix) << ", ";
            }
            ss << ")";
            throw parse_error(ss.str(), token);
        }

        // parse and return the number
        T result = static_cast<T>(0);
        if (!util::parse_number<T>(token, result, number_parts, error))
        {
            throw parse_error("Failed to parse number", error);
        }
        return result;
    }
};

JXC_END_NAMESPACE(conv)


template<>
struct Converter<std::nullptr_t>
{
    using value_type = std::nullptr_t;

    static std::string get_annotation()
    {
        return detail::base_type_annotation<value_type>();
    }

    static void serialize(Serializer& doc, std::nullptr_t)
    {
        doc.value_null();
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        parser.require(TokenType::Null);
        return nullptr;
    }
};


template<>
struct Converter<bool>
{
    using value_type = bool;

    static std::string get_annotation()
    {
        return detail::base_type_annotation<value_type>();
    }

    static void serialize(Serializer& doc, bool value)
    {
        doc.value_bool(value);
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        //NB. conv::require returns the index of the matched token, so we can trivially cast from that index to bool.
        // This requires that the order of the tokens in the initializer_list is { False, True }.
        return static_cast<bool>(parser.require({ TokenType::False, TokenType::True }));
    }
};


template<traits::Character T>
struct Converter<T>
{
    using value_type = T;

    static std::string get_annotation()
    {
        return detail::base_type_annotation<value_type>();
    }

    static void serialize(Serializer& doc, value_type value)
    {
        detail::MiniBuffer<char, 12> buf;
        if constexpr (sizeof(value_type) == sizeof(uint8_t))
        {
            buf[0] = static_cast<char>(value);
            buf[1] = '\0';
            doc.value_string(buf.as_view(1));
        }
        else if constexpr (sizeof(value_type) > sizeof(uint8_t) && sizeof(value_type) <= sizeof(uint32_t))
        {
            size_t buf_index = 0;
            detail::utf8::encode(buf.data(), buf.capacity(), buf_index, static_cast<uint32_t>(value));
            doc.value_string(buf.as_view(buf_index));
        }
        else
        {
            JXC_ASSERTF("Character type larger than expected (sizeof({}) == {})", detail::get_type_name<value_type>(), sizeof(value_type));
        }
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        const Token& tok = parser.value().token;
        ErrorInfo err;
        switch (tok.type)
        {
        case TokenType::String:
        {
            // parse as a string, erroring out if the string is not exactly one character
            size_t req_buffer_size = 0;
            if (!util::get_string_token_required_buffer_size(tok, req_buffer_size, err))
            {
                throw parse_error("Failed parsing string", err);
            }
            else if (req_buffer_size == 0)
            {
                throw parse_error(jxc::format("Empty string invalid for character type {}", get_annotation()), tok);
            }
            else if (req_buffer_size >= 12)
            {
                // req_buffer_size is not the actual string length, but we can still improve perf slightly here
                // by only parsing the entire string if we know it's very short.
                throw parse_error(jxc::format("String too long for character type {}", get_annotation()), tok);
            }
            detail::MiniBuffer<char, 12> buf;
            size_t buf_len = 0;
            if (!util::parse_string_token_to_buffer(tok, buf.data(), buf.capacity(), buf_len, err))
            {
                throw parse_error("Failed parsing string", err);
            }

            if constexpr (sizeof(value_type) == sizeof(char))
            {
                if (buf_len != 1)
                {
                    throw parse_error(jxc::format("Expected {}, got string with length {}", get_annotation(), buf_len), tok);
                }
                return static_cast<value_type>(buf[0]);
            }
            else if constexpr (sizeof(value_type) <= sizeof(uint32_t))
            {
                size_t buf_idx = 0;
                JXC_DEBUG_ASSERT(buf_len <= buf.capacity());
                const uint32_t codepoint = detail::utf8::decode(buf.data(), buf_len, buf_idx);

                if (buf_idx != buf_len)
                {
                    throw parse_error(jxc::format("Got more than one codepoint while parsing string {} to character type {}",
                        detail::debug_string_repr(tok.value.as_view()), get_annotation()), tok);
                }

                if (codepoint <= std::numeric_limits<value_type>::max())
                {
                    return static_cast<value_type>(codepoint);
                }
                else
                {
                    throw parse_error(jxc::format("Codepoint {} too large for character type {}", codepoint, get_annotation()), tok);
                }
            }
            else
            {
                JXC_ASSERTF("Character type larger than expected (sizeof({}) == {})", detail::get_type_name<value_type>(), sizeof(value_type));
            }
        }
        case TokenType::Number:
        {
            // parse as an integer, erroring out if the integer is not valid for the given character type
            int64_t result = 0;
            std::string err_msg;
            if (!util::parse_number_simple(tok.value.as_view(), result, &err_msg))
            {
                throw parse_error(jxc::format("Failed to parse number: {}", err_msg), tok);
            }

            if constexpr (std::same_as<value_type, char>)
            {
                // char is sometimes signed in weird situations.
                // If the number is negative, use c-style behavior where possible: treat it as int8_t and then c-style cast it to char.
                if (result < 0 && result >= std::numeric_limits<int8_t>::min())
                {
                    return (char)static_cast<int8_t>(result);
                }
                else if (result >= 0 && result <= std::numeric_limits<uint8_t>::max())
                {
                    return (char)static_cast<uint8_t>(result);
                }
                throw parse_error(jxc::format("Failed to parse {}: integer value {} is not in range {}..{}", get_annotation(), result,
                    std::numeric_limits<int8_t>::min(), std::numeric_limits<char>::max()), tok);
            }
            else if constexpr (sizeof(value_type) >= sizeof(uint8_t) && sizeof(value_type) <= sizeof(uint32_t))
            {
                if (result >= std::numeric_limits<value_type>::min() && result <= std::numeric_limits<value_type>::max())
                {
                    return static_cast<value_type>(result);
                }
                throw parse_error(jxc::format("Failed to parse {}: integer value {} is not in range {}..{}",
                    get_annotation(), result, std::numeric_limits<value_type>::min(), std::numeric_limits<uint16_t>::max()), tok);
            }
            else
            {
                JXC_ASSERTF("Character type larger than expected (sizeof({}) == {})", detail::get_type_name<value_type>(), sizeof(value_type));
            }
        }
        case TokenType::Null:
        {
            return static_cast<value_type>(0);
        }
        default:
            parser.require(tok.type, { TokenType::String, TokenType::Number, TokenType::Null });
            break;
        }
        return static_cast<value_type>(0);
    }
};


template<std::unsigned_integral T>
    requires (!traits::Character<T>)
struct Converter<T>
{
    using value_type = T;

    static std::string get_annotation()
    {
        return detail::base_type_annotation<value_type>();
    }

    static void serialize(Serializer& doc, value_type value)
    {
        doc.value_uint(static_cast<uint64_t>(value));
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        parser.require(TokenType::Number);
        value_type result = 0;
        ErrorInfo err;
        if (!util::parse_number_simple<value_type>(parser.value().token, result, err))
        {
            throw parse_error("Failed to parse unsigned integer", err);
        }
        return result;
    }
};


template<std::signed_integral T>
    requires (!traits::Character<T>)
struct Converter<T>
{
    using value_type = T;

    static std::string get_annotation()
    {
        return detail::base_type_annotation<value_type>();
    }

    static void serialize(Serializer& doc, value_type value)
    {
        doc.value_int(static_cast<int64_t>(value));
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        parser.require(TokenType::Number);
        value_type result = 0;
        ErrorInfo err;
        if (!util::parse_number_simple<value_type>(parser.value().token, result, err))
        {
            throw parse_error("Failed to parse signed integer", err);
        }
        return result;
    }
};


template<std::floating_point T>
struct Converter<T>
{
    using value_type = T;

    static std::string get_annotation()
    {
        return detail::base_type_annotation<value_type>();
    }

    static void serialize(Serializer& doc, value_type value)
    {
        doc.value_float(static_cast<double>(value));
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        parser.require(TokenType::Number);
        value_type result = 0;
        ErrorInfo err;
        if (!util::parse_number_simple<value_type>(parser.value().token, result, err))
        {
            throw parse_error("Failed to parse floating point number", err);
        }
        return result;
    }
};


template<>
struct Converter<std::string>
{
    using value_type = std::string;

    static std::string get_annotation()
    {
        return "string";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        doc.value_string(value);
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        return parser.parse_token_as_string(parser.value().token);
    }
};


static_assert(ConverterWithSerialize<std::string>, "std::string has serialize");
static_assert(ConverterWithParse<std::string>, "std::string has parse");


// converter for strings stored as a vector of characters
template<>
struct Converter<std::vector<char>>
{
    using value_type = std::vector<char>;

    static std::string get_annotation()
    {
        return "string";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        doc.value_string(std::string_view{ value.data(), value.size() });
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        value_type result;
        parser.require({ TokenType::ByteString, TokenType::String, TokenType::Identifier });
        const Token& token = parser.value().token;
        ErrorInfo err;
        if (token.type == TokenType::Identifier)
        {
            // copy the identifier string directly into the output vector
            result.resize(token.value.size());
            JXC_STRNCPY(result.data(), result.size(), token.value.data(), token.value.size());
        }
        else if (token.type == TokenType::ByteString)
        {
            // make an adapter that allows parse_bytes_token to work with vector<char>
            struct Adapter
            {
                std::vector<char>& container;
                inline void resize(size_t new_size) { container.resize(new_size); }
                inline uint8_t* data() { return reinterpret_cast<uint8_t*>(container.data()); }
                inline size_t size() const { return container.size(); }
            };

            auto adapter = Adapter{ result };
            if (!util::parse_bytes_token<Adapter>(token, adapter, err))
            {
                throw parse_error("Failed to parse array of characters from bytes", err);
            }
        }
        else
        {
            JXC_DEBUG_ASSERT(token.type == TokenType::String);
            if (!util::parse_string_token<value_type>(token, result, err))
            {
                throw parse_error("Failed to parse string value to bytes", err);
            }
        }
        
        return result;
    }
};


template<>
struct Converter<std::vector<uint8_t>>
{
    using value_type = std::vector<uint8_t>;

    static std::string get_annotation()
    {
        return "bytes";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        doc.value_bytes(value.data(), value.size());
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        value_type result;
        parser.require(TokenType::ByteString);
        ErrorInfo err;
        if (!util::parse_bytes_token<value_type>(parser.value().token, result, err))
        {
            throw parse_error("Failed to parse bytes", err);
        }
        return result;
    }
};


template<>
struct Converter<Date>
{
    using value_type = Date;

    static std::string get_annotation()
    {
        return "date";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        doc.value_date(value);
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        value_type result;
        ErrorInfo err;
        parser.require(TokenType::DateTime);
        if (!util::parse_date_token(parser.value().token, result, err))
        {
            throw parse_error("Failed to parse date", err);
        }
        return result;
    }
};


template<>
struct Converter<DateTime>
{
    using value_type = DateTime;

    static std::string get_annotation()
    {
        return "datetime";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        doc.value_datetime(value);
    }

    static value_type parse(conv::Parser& parser, TokenSpan generic_anno)
    {
        value_type result;
        parser.require(TokenType::DateTime);
        ErrorInfo err;
        if (!util::parse_datetime_token(parser.value().token, result, err))
        {
            throw parse_error("Failed to parse datetime", err);
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
T parse(std::string_view jxc_source)
{
    conv::Parser parser(jxc_source);
    if (!parser.next())
    {
        if (parser.has_error())
        {
            jxc::ErrorInfo err = parser.get_error();
            err.get_line_and_col_from_buffer(parser.get_buffer());
            throw parse_error("Parse error", err);
        }
        else
        {
            throw parse_error("Unexpected end of stream");
        }
    }

    return parser.parse_value<T>();
}


template<typename T>
std::optional<T> try_parse(std::string_view jxc_source, ErrorInfo* out_error = nullptr)
{
    conv::Parser parser(jxc_source);

    try
    {
        // advance exactly once, then parse the requested type
        parser.require_next();
        return parser.parse_value<T>();
    }
    catch (const parse_error& err)
    {
        if (out_error != nullptr)
        {
            if (err.has_error_info())
            {
                *out_error = err.get_error();
                out_error->get_line_and_col_from_buffer(parser.get_buffer());
            }
            else
            {
                *out_error = ErrorInfo(err.what());
            }
        }
    }

    return std::nullopt;
}

JXC_END_NAMESPACE(conv)

JXC_END_NAMESPACE(jxc)
