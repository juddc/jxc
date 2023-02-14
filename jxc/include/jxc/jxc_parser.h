#pragma once
#include <string_view>
#include <charconv>
#include <cmath>
#include "jxc/jxc_util.h"
#include "jxc/jxc_lexer.h"


JXC_BEGIN_NAMESPACE(jxc)


enum class ElementType : uint8_t
{
    Invalid = 0,
    Number,
    Bool,
    Null,
    Bytes,
    String,
    DateTime,
    ExpressionToken,
    Comment,
    BeginArray,
    EndArray,
    BeginExpression,
    EndExpression,
    BeginObject,
    ObjectKey,
    EndObject,
};


const char* element_type_to_string(ElementType type);


inline std::ostream& operator<<(std::ostream& os, ElementType type)
{
    return (os << element_type_to_string(type));
}


/// Returns true if a given element type can contain one or more value tokens
/// (eg. a Number element always contains a value token, but a Null element does not require one)
inline bool element_can_contain_value(ElementType type)
{
    switch (type)
    {
    case ElementType::Null:
    case ElementType::BeginArray:
    case ElementType::EndArray:
    case ElementType::BeginObject:
    case ElementType::EndObject:
    case ElementType::BeginExpression:
    case ElementType::EndExpression:
        return false;

    case ElementType::Number:
    case ElementType::Bool:
    case ElementType::Bytes:
    case ElementType::String:
    case ElementType::DateTime:
    case ElementType::ExpressionToken:
    case ElementType::Comment:
    case ElementType::ObjectKey:
        return true;

    default:
        break;
    }
    return false;
}


/// Returns true if a given element type is allowed to have an annotation
inline bool element_can_contain_annotation(ElementType type)
{
    switch (type)
    {
    case ElementType::Null:
    case ElementType::Number:
    case ElementType::Bool:
    case ElementType::Bytes:
    case ElementType::String:
    case ElementType::DateTime:
    case ElementType::BeginArray:
    case ElementType::BeginObject:
    case ElementType::BeginExpression:
        return true;

    case ElementType::EndArray:
    case ElementType::EndObject:
    case ElementType::EndExpression:
    case ElementType::ExpressionToken:
    case ElementType::Comment:
    case ElementType::ObjectKey:
        return false;

    default:
        break;
    }
    return false;
}

/// Returns true if a given element is a valid value type (includes container-begin elements)
inline bool element_is_value_type(ElementType type)
{
    switch (type)
    {
    case ElementType::Number:
    case ElementType::Bool:
    case ElementType::Null:
    case ElementType::Bytes:
    case ElementType::String:
    case ElementType::DateTime:
    case ElementType::BeginArray:
    case ElementType::BeginExpression:
    case ElementType::BeginObject:
        return true;

    case ElementType::Comment:
    case ElementType::ExpressionToken:
    case ElementType::EndArray:
    case ElementType::EndExpression:
    case ElementType::ObjectKey:
    case ElementType::EndObject:
    case ElementType::Invalid:
    default:
        break;
    }
    return false;
}


/// Returns true if a given element is valid for use inside an expression container
inline bool element_is_expression_value_type(ElementType type)
{
    switch (type)
    {
    case ElementType::Number:
    case ElementType::Bool:
    case ElementType::Null:
    case ElementType::Bytes:
    case ElementType::String:
    case ElementType::DateTime:
    case ElementType::ExpressionToken:
    case ElementType::Comment:
        return true;

    case ElementType::BeginArray:
    case ElementType::EndArray:
    case ElementType::BeginExpression:
    case ElementType::EndExpression:
    case ElementType::BeginObject:
    case ElementType::EndObject:
    case ElementType::ObjectKey:
    case ElementType::Invalid:
    default:
        break;
    }
    return false;
}


template<typename T>
struct TElement
{
    using anno_type = T;

    ElementType type = ElementType::Invalid;
    Token token;
    anno_type annotation;

    TElement() = default;

    TElement(ElementType type)
        : type(type)
    {
    }

    TElement(ElementType type, const Token& token, const anno_type& annotation)
        : type(type)
        , token(token)
        , annotation(annotation)
    {
    }

    TElement(const TElement& rhs) = default;
    TElement(TElement&& rhs) = default;
    TElement& operator=(const TElement& rhs) = default;
    TElement& operator=(TElement&& rhs) = default;

    template<typename RhsT>
    inline bool operator==(const TElement<RhsT>& rhs) const { return type == rhs.type && token == rhs.token && annotation == rhs.annotation; }

    template<typename RhsT>
    inline bool operator!=(const TElement<RhsT>& rhs) const { return !operator==(rhs); }

    inline void reset()
    {
        type = ElementType::Invalid;
        token.reset();
        annotation.reset();
    }

    inline TElement<TokenSpan> view() const
    {
        if constexpr (std::is_same_v<anno_type, TokenSpan>)
        {
            return { type, token.view(), annotation };
        }
        else
        {
            return { type, token.view(), annotation.to_token_span() };
        }
    }

    inline TElement<OwnedTokenSpan> copy() const
    {
        return { type, token.copy(), OwnedTokenSpan(annotation) };
    }

private:
    std::string format_string(bool use_repr) const
    {
        const bool with_value = element_can_contain_value(type) && token.value.size() > 0;
        const bool with_anno = element_can_contain_annotation(type) && annotation.size() > 0;
        if (!with_value && !with_anno)
        {
            return jxc::format("Element.{}", element_type_to_string(type));
        }
        else if (with_value && !with_anno)
        {
            return jxc::format("Element.{}({})", element_type_to_string(type),
                use_repr ? token.to_repr() : token.to_string());
        }
        else if (!with_value && with_anno)
        {
            return jxc::format("Element.{}(`{}`)", element_type_to_string(type),
                use_repr ? annotation.to_repr() : annotation.to_string());
        }
        else
        {
            return jxc::format("Element.{}(`{}` {})", element_type_to_string(type),
                use_repr ? annotation.to_repr() : annotation.to_string(),
                use_repr ? token.to_repr() : token.to_string());
        }
    }

public:
    inline std::string to_string() const { return format_string(false); }
    inline std::string to_repr() const { return format_string(true); }
};


using Element = TElement<TokenSpan>;
using OwnedElement = TElement<OwnedTokenSpan>;


class JumpParser
{
private:
    enum JumpState : uint8_t
    {
        JS_Value = 0,
        JS_Array,
        JS_Expr,
        JS_Object,
    };

    enum ContainerState : uint8_t
    {
        CS_None = 0,
        OBJ_Begin,
        OBJ_Key,
        OBJ_Value,
    };

    struct JumpStackVars
    {
        JumpState state;
        ContainerState container_state;
        int64_t container_size;
        int32_t paren_depth;
        int32_t square_bracket_depth;
        int32_t brace_depth;

        static JumpStackVars make(JumpState state = JS_Value, ContainerState container_state = OBJ_Begin)
        {
            return { state, container_state, 0, 0, 0, 0 };
        }

        static constexpr int32_t max_bracket_depth = std::numeric_limits<int32_t>::max() - 2;
    };

    std::string_view buffer;
    Lexer lexer;
    ErrorInfo error;
    Token tok;
    Element current_value;

    detail::ArrayBuffer<Token, 32> annotation_buffer;
    detail::ArrayBuffer<JumpStackVars, 96> jump_stack;

    JumpStackVars* jump_vars = nullptr;

    inline void jump_stack_push(JumpState new_state, ContainerState new_container_state = CS_None)
    {
        jump_stack.push(JumpStackVars::make(new_state, new_container_state));
        jump_vars = &jump_stack.back();
    }

    inline void jump_stack_pop()
    {
        switch (jump_stack.size())
        {
        case 0:
            JXC_DEBUG_ASSERTF(jump_stack.size() > 0, "jump_stack_pop() called with empty stack");
            break;
        case 1:
            jump_stack.pop_back();
            jump_vars = nullptr;
            break;
        default:
            jump_stack.pop_back();
            jump_vars = &jump_stack.back();
            break;
        }
    }

    JXC_FORCEINLINE bool advance()
    {
        return lexer.next(tok, error);
    }

    JXC_FORCEINLINE bool advance_skip_comments()
    {
        if (!lexer.next(tok, error)) { return false; }
        while (tok.type == TokenType::Comment)
        {
            if (!lexer.next(tok, error)) { return false; }
        }
        return true;
    }

    JXC_FORCEINLINE bool skip_over_line_breaks()
    {
        while (tok.type == TokenType::LineBreak || tok.type == TokenType::Comment)
        {
            if (!lexer.next(tok, error)) { return false; }
        }
        return true;
    }

    bool advance_separator(TokenType container_close_type, const char* cur_jump_block_name);

public:
    JumpParser(std::string_view buffer)
        : buffer(buffer)
        , lexer(buffer.data(), buffer.size())
    {
    }

    void reset(std::string_view new_buffer)
    {
        buffer = new_buffer;
        lexer = Lexer{ buffer.data(), buffer.size() };
        error = ErrorInfo{};
        annotation_buffer.clear();
        jump_stack.clear();
    }

    bool next();

    const Element& value() const { return current_value; }

    std::string_view get_buffer() const { return buffer; }

    inline bool has_error() const { return error.is_err; }

    inline size_t stack_depth() const { return jump_stack.size(); }

    inline const ErrorInfo& get_error() const { return error; }

    // profiler requires compilation with JXC_ENABLE_JUMP_BLOCK_PROFILER set to 1
    static void reset_profiler();
    static std::string get_profiler_results(bool sort_by_runtime = true);
};


JXC_BEGIN_NAMESPACE(util)


static constexpr std::string_view raw_string_prefix = "r";
static constexpr std::string_view base64_string_prefix = "b64";

inline bool parse_bool(std::string_view value, bool& out_result)
{
    if (value.size() == 4 && value[0] == 't' && value[1] == 'r' && value[2] == 'u' && value[3] == 'e')
    {
        out_result = true;
        return true;
    }
    else if (value.size() == 5 && value[0] == 'f' && value[1] == 'a' && value[2] == 'l' && value[3] == 's' && value[4] == 'e')
    {
        out_result = false;
        return true;
    }
    return false;
}


inline bool is_decimal_digit(char ch) { return ch >= '0' && ch <= '9'; }
inline bool is_hex_digit(char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }
inline bool is_octal_digit(char ch) { return ch >= '0' && ch <= '7'; }
inline bool is_binary_digit(char ch) { return ch == '0' || ch == '1'; }


inline int32_t char_to_int(char ch)
{
    switch (ch)
    {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    default: break;
    }
    JXC_ASSERTF(is_decimal_digit(ch), "char_to_int requires a decimal digit char, got {}", detail::debug_char_repr(ch));
    return 0;
}


bool string_is_number_base_10(std::string_view value);


bool string_to_double(std::string_view value, double& out_value);


template<typename T>
inline bool string_to_float(std::string_view value, T& out_value)
{
    static_assert(std::is_floating_point_v<T>);
    if constexpr (std::is_same_v<T, double>)
    {
        return string_to_double(value, out_value);
    }
    else
    {
        double result = 0.0;
        if (string_to_double(value, result))
        {
            out_value = static_cast<T>(result);
            return true;
        }
        return false;
    }
}


// converts a string with a base-10 numeric value to any numeric type (int or float)
template<typename T>
inline bool string_to_number_decimal(std::string_view value, T& out_result)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "string_to_number_decimal requires a numeric type");
    if (value.size() == 0)
    {
        return false;
    }

    JXC_DEBUG_ASSERT(string_is_number_base_10(value));

    // fast-path for single-character decimal numbers
    if (value.size() == 1)
    {
        if (is_decimal_digit(value[0]))
        {
            out_result = static_cast<T>(char_to_int(value[0]));
            return true;
        }
        return false;
    }
    else if (value.size() == 2 && value[0] == '-')
    {
        if constexpr (std::is_unsigned_v<T>)
        {
            out_result = static_cast<T>(0);
            return value[1] == '0'; // unsigned conversion is only valid for `-0`
        }
        else
        {
            if (is_decimal_digit(value[1]))
            {
                out_result = static_cast<T>(-char_to_int(value[1]));
                return true;
            }
            else
            {
                out_result = static_cast<T>(0);
                return false;
            }
        }
    }

    if constexpr (std::is_floating_point_v<T>)
    {
        // can use this instead if you want to get rid of the fast_float dependency (it's significantly slower though)
        //out_result = static_cast<T>(strtold(value.data(), nullptr));

        return string_to_float<T>(value, out_result);
    }
    else
    {
        out_result = static_cast<T>(strtoll(value.data(), nullptr, 10));
    }

    return true;
}


template<typename T>
inline bool string_to_number_hex(std::string_view value, T& out_result)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "string_to_number_hex requires a numeric type");
    if (value.size() == 0)
    {
        return false;
    }
#if JXC_DEBUG
    // verify that all digits are hex
    for (size_t i = 0; i < value.size(); i++)
    {
        if (!is_hex_digit(value[i]))
        {
            return false;
        }
    }
#endif
    out_result = static_cast<T>(strtoll(value.data(), nullptr, 16));
    return true;
}


template<typename T>
inline bool string_to_number_octal(std::string_view value, T& out_result)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "string_to_number_octal requires a numeric type");
    if (value.size() == 0)
    {
        return false;
    }
#if JXC_DEBUG
    // verify that all digits are octal
    for (size_t i = 0; i < value.size(); i++)
    {
        if (!is_octal_digit(value[i]))
        {
            return false;
        }
    }
#endif
    out_result = static_cast<T>(strtol(value.data(), nullptr, 8));
    return true;
}


template<typename T>
inline bool string_to_number_binary(std::string_view value, T& out_result)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "string_to_number_binary requires a numeric type");
    if (value.size() == 0)
    {
        return false;
    }
#if JXC_DEBUG
    // verify that all digits are binary
    for (size_t i = 0; i < value.size(); i++)
    {
        if (!is_binary_digit(value[i]))
        {
            return false;
        }
    }
#endif
    out_result = static_cast<T>(strtol(value.data(), nullptr, 2));
    return true;
}


inline bool is_number_token_negative(std::string_view number_value)
{
    return number_value.size() > 0 && number_value.front() == '-';
}


inline bool is_number_token_negative(const Token& number_token)
{
    return number_token.value.front() == '-';
}


struct NumberTokenSplitResult
{
    char sign = '\0';
    std::string_view prefix;
    std::string_view value;
    int32_t exponent = 0;
    std::string_view suffix;
    FloatLiteralType float_type = FloatLiteralType::Finite;

    inline bool is_integer() const
    {
        return float_type == FloatLiteralType::Finite && exponent >= 0 && value.find_first_of('.') == std::string_view::npos;
    }

    inline bool is_floating_point() const
    {
        return float_type != FloatLiteralType::Finite || exponent < 0 || value.find_first_of('.') != std::string_view::npos;
    }
};


bool split_number_token_value(const Token& number_token, NumberTokenSplitResult& out_result, ErrorInfo& out_error);


template<typename T>
bool parse_number(const Token& tok, T& out_value, const NumberTokenSplitResult& number, ErrorInfo& out_error)
{
    auto apply_sign = [&tok](char ch, T& val, ErrorInfo& out_err) -> bool
    {
        if (ch == '-')
        {
            if constexpr (std::is_unsigned_v<T>)
            {
                out_err = ErrorInfo{ "parse_number got an unsigned type, but the number is negative", tok.start_idx, tok.end_idx };
                return false;
            }
            else
            {
                val = -val;
            }
        }
        return true;
    };

    if constexpr (std::is_floating_point_v<T>)
    {
        switch (number.float_type)
        {
        case FloatLiteralType::Finite:
            break;
        case FloatLiteralType::NotANumber:
            out_value = std::numeric_limits<T>::quiet_NaN();
            JXC_DEBUG_ASSERT(get_float_literal_type(out_value) == FloatLiteralType::NotANumber);
            return true;
        case FloatLiteralType::PosInfinity:
            out_value = std::numeric_limits<T>::infinity();
            JXC_DEBUG_ASSERT(get_float_literal_type(out_value) == FloatLiteralType::PosInfinity);
            return true;
        case FloatLiteralType::NegInfinity:
            out_value = -std::numeric_limits<T>::infinity();
            JXC_DEBUG_ASSERT(get_float_literal_type(out_value) == FloatLiteralType::NegInfinity);
            return true;
        default:
            JXC_ASSERTF(false, "Invalid FloatLiteralType {}", static_cast<int>(number.float_type));
            break;
        }
    }
    else
    {
        if (number.float_type != FloatLiteralType::Finite)
        {
            out_error = ErrorInfo{
                jxc::format("Value {} cannot be converted to a non-floating point value.", float_literal_type_to_string(number.float_type)),
                tok.start_idx, tok.end_idx
            };
            return false;
        }
    }

    if (number.prefix.size() == 2)
    {
        switch (number.prefix[1])
        {
        case 'x':
        case 'X':
            if (string_to_number_hex<T>(number.value, out_value))
            {
                return apply_sign(number.sign, out_value, out_error);
            }
            else
            {
                out_error = ErrorInfo{ jxc::format("Value {} is not a valid hex literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
                return false;
            }
        case 'b':
        case 'B':
            if (string_to_number_binary<T>(number.value, out_value))
            {
                return apply_sign(number.sign, out_value, out_error);
            }
            else
            {
                out_error = ErrorInfo{ jxc::format("Value {} is not a valid binary literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
                return false;
            }
        case 'o':
        case 'O':
            if (string_to_number_octal<T>(number.value, out_value))
            {
                return apply_sign(number.sign, out_value, out_error);
            }
            else
            {
                out_error = ErrorInfo{ jxc::format("Value {} is not a valid octal literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
                return false;
            }
        default:
            break;
        }

        return false;
    }

    if (!string_to_number_decimal<T>(number.value, out_value))
    {
        out_error = ErrorInfo{ jxc::format("Value {} is not a valid decimal literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
        return false;
    }

    if (!apply_sign(number.sign, out_value, out_error))
    {
        return false;
    }

    if (number.exponent > 0)
    {
        int32_t exp = number.exponent;
        while (exp > 0)
        {
            out_value *= 10;
            --exp;
        }
    }
    else if (number.exponent < 0)
    {
        if constexpr (std::is_integral_v<T>)
        {
            out_error = ErrorInfo{ "parse_number got an integer type, but the number has a negative exponent", tok.start_idx, tok.end_idx };
            return false;
        }
        else
        {
            out_value = static_cast<T>(std::pow<T>(out_value, number.exponent));
        }
    }

    return true;
}


// General-purpose number parsing function.
// Also an example of how to use split_number_token_value and parse_number together.
template<typename NumberType, typename SuffixStringType = std::string>
bool parse_number_simple(std::string_view value_string, NumberType& out_value, std::string* out_error = nullptr, SuffixStringType* out_suffix = nullptr)
{
    Token tok = Token(TokenType::Number, 0, value_string.size() - 1, FlexString::make_view(value_string));
    NumberTokenSplitResult number;
    ErrorInfo err;
    if (!split_number_token_value(tok, number, err))
    {
        if (out_error != nullptr)
        {
            *out_error = err.to_string(value_string);
        }
        return false;
    }
    if (!parse_number<NumberType>(tok, out_value, number, err))
    {
        if (out_error != nullptr)
        {
            *out_error = err.to_string(value_string);
        }
        return false;
    }
    if (out_suffix != nullptr)
    {
        *out_suffix = SuffixStringType(number.suffix);
    }
    return true;
}


// Number parsing function specific to object keys, with the appropriate error handling
template<typename IntType, typename SuffixStringType = std::string>
bool parse_number_object_key(const Token& tok, IntType& out_value, ErrorInfo& out_error, SuffixStringType* out_suffix = nullptr)
{
    static_assert(std::is_integral_v<IntType>, "Numeric object keys must be integers");

    NumberTokenSplitResult number;
    if (!split_number_token_value(tok, number, out_error))
    {
        return false;
    }

    if (number.is_floating_point())
    {
        out_error = ErrorInfo("Floating point values are not allowed as object keys", tok.start_idx, tok.end_idx);
        return false;
    }

    if (out_suffix != nullptr)
    {
        *out_suffix = SuffixStringType(number.suffix);
    }

    return parse_number<IntType>(tok, out_value, number, out_error);
}


// Returns a string_view representing a string token's inner string data.
// eg. The string without quote chars, heredoc+parens, etc.
// This is half of the parsing process for strings - the other half is handling escape characters.
bool string_token_to_value(const Token& string_token, std::string_view& out_view, bool& out_is_raw_string, ErrorInfo& out_error);

// Checks if a string contains any backslash characters
bool string_has_escape_chars(std::string_view string_value);

// Returns the required buffer size to store a raw string, based on the string's value (see string_token_to_value())
inline size_t get_raw_string_required_buffer_size(std::string_view string_value) { return string_value.size(); }

// Returns the required buffer size to store a string, based on the string's value (see string_token_to_value())
// This scans the string and checks escape characters for how many bytes they require.
// Note that for strings with a large number of unicode escapes, this can overestimate a bit.
size_t get_string_required_buffer_size(std::string_view string_value);

// Returns the required buffer size to store a string, based on the string's value (see string_token_to_value())
inline size_t get_string_required_buffer_size(std::string_view string_value, bool is_raw_string)
{
    return is_raw_string ? get_raw_string_required_buffer_size(string_value) : get_string_required_buffer_size(string_value);
}

// Returns the required buffer size to store a string.
bool get_string_token_required_buffer_size(const Token& string_token, size_t& out_buffer_size, ErrorInfo& out_error);

// Scans a string value and parses its contents for escape characters, storing them in the output buffer.
// Use string_token_to_value() to convert a token to a string value, and get_string_required_buffer_size() to compute the buffer size needed.
// Note that this does not need to be called for raw strings.
bool parse_string_escapes_to_buffer(std::string_view string_value, size_t string_token_start_idx, size_t string_token_end_idx, char* out_string_buffer,
    size_t string_buffer_size, size_t& out_num_chars_written, ErrorInfo& out_error);

// Parses a string token to a string buffer. Use get_string_token_required_buffer_size() to compute the size of the output buffer.
bool parse_string_token_to_buffer(const Token& string_token, char* out_buffer, size_t buffer_size, size_t& out_num_chars_written, ErrorInfo& out_error);

// Parses a string token to a resizable string buffer (eg. std::string or std::vector<char>)
template<JXC_CONCEPT(traits::StringBuffer) T>
bool parse_string_token(const Token& string_token, T& out_string_buffer, ErrorInfo& out_error)
{
    std::string_view value;
    bool is_raw = false;
    if (!string_token_to_value(string_token, value, is_raw, out_error))
    {
        return false;
    }

    const size_t req_buf_size = get_string_required_buffer_size(value, is_raw);
    out_string_buffer.resize(req_buf_size);
    if (req_buf_size == 0)
    {
        return true;
    }

    char* buf_ptr = out_string_buffer.data();
    const size_t buf_len = out_string_buffer.size();

    JXC_ASSERTF(buf_len >= req_buf_size, "Output string buffer with size {} is too small to fit string of size {}", buf_len, req_buf_size);

    // string is not raw and has escape chars, so we need to parse the escape characters
    if (!is_raw && string_has_escape_chars(value))
    {
        size_t num_chars_written = 0;
        if (!parse_string_escapes_to_buffer(value, string_token.start_idx, string_token.end_idx, buf_ptr, buf_len, num_chars_written, out_error))
        {
            return false;
        }
        out_string_buffer.resize(num_chars_written);
        return true;
    }

    // should always be true - will only fail if there's a bug in one of the get_*_required_buffer_size() functions
    JXC_DEBUG_ASSERT(value.size() <= buf_len);

    // raw string or no escape chars - we can just use memcpy and be done
    JXC_MEMCPY(buf_ptr, buf_len, value.data(), value.size());
    return true;
}

// returns the number of bytes required to store the decoded version of a byte string
size_t get_byte_buffer_required_size(const char* bytes_token_str, size_t bytes_token_str_len);

bool parse_bytes_token(const Token& bytes_token, uint8_t* out_data_buffer, size_t out_data_buffer_size, size_t& out_num_bytes_written, ErrorInfo& out_error);

template<JXC_CONCEPT(traits::ByteBuffer) T>
bool parse_bytes_token(const Token& bytes_token, T& out_data, ErrorInfo& out_error)
{
    out_data.resize(bytes_token.value.size());
    size_t num_bytes_written = 0;
    if (parse_bytes_token(bytes_token, out_data.data(), out_data.size(), num_bytes_written, out_error))
    {
        out_data.resize(num_bytes_written);
        return true;
    }
    return false;
}

// Checks if the given datetime token is just a date (no time data)
bool datetime_token_is_date(const Token& datetime_token);

// Checks if the given datetime token includes both date and time data
bool datetime_token_is_datetime(const Token& datetime_token);

// Parses a date token into a Date value. This will return an error if the token includes time data.
bool parse_date_token(const Token& datetime_token, Date& out_date, ErrorInfo& out_error);

// Parses a date token into a DateTime value.
// If require_time_data is true, this will return an error if the token does not include time info.
// If require_time_data is false and the token does not include time data, out_datetime will have a time of 00:00:00Z.
bool parse_datetime_token(const Token& datetime_token, DateTime& out_datetime, ErrorInfo& out_error, bool require_time_data = false);

JXC_END_NAMESPACE(util)

JXC_END_NAMESPACE(jxc)
