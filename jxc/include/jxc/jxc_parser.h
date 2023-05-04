#pragma once
#include <string_view>
#include <charconv>
#include <cmath>
#include "jxc/jxc_util.h"
#include "jxc/jxc_stack_vector.h"
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


JXC_EXPORT const char* element_type_to_string(ElementType type);


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


JXC_BEGIN_NAMESPACE(util)

inline bool is_decimal_digit(char ch) { return ch >= '0' && ch <= '9'; }
inline bool is_hex_digit(char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }
inline bool is_octal_digit(char ch) { return ch >= '0' && ch <= '7'; }
inline bool is_binary_digit(char ch) { return ch == '0' || ch == '1'; }

JXC_END_NAMESPACE(util)


template<typename T>
struct TElement
{
    using anno_type = T;
    static constexpr bool owned_data = std::is_same_v<anno_type, TokenList>;

    ElementType type = ElementType::Invalid;
    Token token;
    anno_type annotation;

    TElement() = default;

    TElement(ElementType in_type)
        : type(in_type)
    {
    }

    TElement(ElementType in_type, const Token& in_token, const anno_type& in_annotation)
        : type(in_type)
        , token(in_token)
        , annotation(in_annotation)
    {
        if constexpr (owned_data)
        {
            token.to_owned_inplace();
        }
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

    inline TElement<TokenView> view() const
    {
        if constexpr (std::is_same_v<anno_type, TokenView>)
        {
            return { type, token.view(), annotation };
        }
        else
        {
            return { type, token.view(), TokenView(annotation) };
        }
    }

    inline TElement<TokenList> copy() const
    {
        return { type, token.copy(), TokenList(annotation) };
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


using Element = TElement<TokenView>;
using OwnedElement = TElement<TokenList>;


class JXC_EXPORT JumpParser
{
protected:
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

protected:
    ErrorInfo error;

private:
    std::string_view buffer;
    Lexer lexer;
    Token tok;
    Element current_value;

    detail::StackVector<Token, 32> annotation_buffer;
    detail::StackVector<JumpStackVars, 96> jump_stack;

    JumpStackVars* jump_vars = nullptr;

    inline std::string_view get_annotation_buffer_source_view() const
    {
        if (annotation_buffer.size() > 0)
        {
            const size_t start_idx = annotation_buffer[0].start_idx;
            const size_t end_idx = annotation_buffer[annotation_buffer.size() - 1].end_idx;
            JXC_DEBUG_ASSERT(end_idx >= start_idx);
            return buffer.substr(start_idx, end_idx - start_idx);
        }
        return std::string_view{};
    }

    inline void jump_stack_push(JumpState new_state, ContainerState new_container_state = CS_None)
    {
        jump_stack.push_back(JumpStackVars::make(new_state, new_container_state));
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

    JXC_FORCEINLINE bool lexer_advance()
    {
        return lexer.next(tok, error);
    }

    JXC_FORCEINLINE bool lexer_advance_skip_comments()
    {
        if (!lexer.next(tok, error)) { return false; }
        while (tok.type == TokenType::Comment)
        {
            if (!lexer.next(tok, error)) { return false; }
        }
        return true;
    }

    JXC_FORCEINLINE bool lexer_skip_over_line_breaks()
    {
        while (tok.type == TokenType::LineBreak || tok.type == TokenType::Comment)
        {
            if (!lexer.next(tok, error)) { return false; }
        }
        return true;
    }

    bool lexer_advance_separator(TokenType container_close_type, const char* cur_jump_block_name);

public:
    JumpParser() = default;

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


// Utility parser that can be used to simpify parsing annotation TokenView values
struct JXC_EXPORT AnnotationParser
{
    TokenView anno;
    size_t idx = 0;
    int64_t paren_depth = 0;
    int64_t angle_bracket_depth = 0;
    ErrorInfo err;
    std::function<void(const ErrorInfo&)> on_error_callback;

    explicit AnnotationParser(TokenView anno, const std::function<void(const ErrorInfo&)>& on_error_callback = nullptr);

private:
    void set_error(std::string&& err_msg, size_t start_idx = invalid_idx, size_t end_idx = invalid_idx);

public:
    inline bool has_error() const { return err.is_err; }

    bool advance();

    bool advance_required();

    inline bool done() const { return idx >= anno.size(); }

    bool done_required();

    inline const Token& current() const { JXC_ASSERT(idx < anno.size()); return anno[idx]; }

    bool require(TokenType tok_type, std::string_view tok_value = std::string_view{});

    bool require_then_advance(TokenType tok_type, std::string_view tok_value = std::string_view{});

    // Return true if the current token matches the given type and value.
    // Same as require() but without setting an error on failure.
    inline bool equals(TokenType tok_type, std::string_view tok_value = std::string_view{}) const
    {
        return !done() && anno[idx].type == tok_type && (tok_value.size() == 0 || anno[idx].value == tok_value);
    }

    // Call this only when the current token type is AngleBracketOpen or Comma.
    // Skips over an inner generic value, stopping at either a matching AngleBracketClose, or a Comma at the same depth.
    // Returns a TokenView for the tokens skipped.
    TokenView skip_over_generic_value();
};


JXC_BEGIN_NAMESPACE(detail)

template<typename T>
inline T sign_char_to_multiplier(char sign_char)
{
    if constexpr (std::is_unsigned_v<T>)
    {
        JXC_DEBUG_ASSERT(sign_char == '+');
        return 1;
    }
    else
    {
        return (sign_char == '-') ? static_cast<T>(-1) : static_cast<T>(1);
    }
}

// Given two decimal integer strings, returns lhs <= rhs.
// This function assumes clean inputs: both values must be at least 1 character and with no leading zeroes.
// Values should have no sign character.
JXC_EXPORT bool decimal_integer_string_less_than_or_equal(std::string_view lhs, std::string_view rhs);

// Checks if an integer in string form is less than or equal to the max value for that type.
// Takes a value without a sign char that is assumed to be positive.
template<typename T>
inline bool value_lte_int_max(std::string_view value)
{
    static_assert(std::is_integral_v<T>);

    if (value.size() == 0)
    {
        return true;
    }

    // all chars must be decimal digits
#if JXC_DEBUG
    for (size_t i = 0; i < value.size(); i++)
    {
        JXC_ASSERT(util::is_decimal_digit(value[i]));
    }
#endif

    if constexpr (std::is_same_v<T, uint8_t>) { return decimal_integer_string_less_than_or_equal(value, "255"); }
    else if constexpr (std::is_same_v<T, uint16_t>) { return decimal_integer_string_less_than_or_equal(value, "65535"); }
    else if constexpr (std::is_same_v<T, uint32_t>) { return decimal_integer_string_less_than_or_equal(value, "4294967295"); }
    else if constexpr (std::is_same_v<T, uint64_t>) { return decimal_integer_string_less_than_or_equal(value, "18446744073709551615"); }
    else if constexpr (std::is_same_v<T, int8_t>) { return decimal_integer_string_less_than_or_equal(value, "127"); }
    else if constexpr (std::is_same_v<T, int16_t>) { return decimal_integer_string_less_than_or_equal(value, "32767"); }
    else if constexpr (std::is_same_v<T, int32_t>) { return decimal_integer_string_less_than_or_equal(value, "2147483647"); }
    else if constexpr (std::is_same_v<T, int64_t>) { return decimal_integer_string_less_than_or_equal(value, "9223372036854775807"); }

    return false;
}

// Checks if an integer in string form is greater than or equal to the min value for that type.
// Takes a value without a sign char that is assumed to be negative.
template<typename T>
inline bool value_gte_int_min(std::string_view value)
{
    static_assert(std::is_integral_v<T> && std::is_signed_v<T>);

    if (value.size() == 0)
    {
        return true;
    }

    // all chars must be decimal digits
#if JXC_DEBUG
    for (size_t i = 0; i < value.size(); i++)
    {
        JXC_ASSERT(util::is_decimal_digit(value[i]));
    }
#endif

    if constexpr (std::is_same_v<T, int8_t>) { return decimal_integer_string_less_than_or_equal(value, "128"); }
    else if constexpr (std::is_same_v<T, int16_t>) { return decimal_integer_string_less_than_or_equal(value, "32768"); }
    else if constexpr (std::is_same_v<T, int32_t>) { return decimal_integer_string_less_than_or_equal(value, "2147483648"); }
    else if constexpr (std::is_same_v<T, int64_t>) { return decimal_integer_string_less_than_or_equal(value, "9223372036854775808"); }

    return false;
}


// determines if a signed integer would fit in a smaller signed integer type
template<typename T>
constexpr bool signed_integer_value_fits_in_type(int64_t value)
{
    static_assert(std::is_integral_v<T>, "signed_integer_value_fits_in_type requires an integer type");
    static_assert(std::is_signed_v<T>, "signed_integer_value_fits_in_type requires a signed integer type");

    if constexpr (std::is_same_v<T, int8_t>)
    {
        return value >= static_cast<int64_t>(INT8_MIN) && value <= static_cast<int64_t>(INT8_MAX);
    }
    else if constexpr (std::is_same_v<T, int16_t>)
    {
        return value >= static_cast<int64_t>(INT16_MIN) && value <= static_cast<int64_t>(INT16_MAX);
    }
    else if constexpr (std::is_same_v<T, int32_t>)
    {
        return value >= static_cast<int64_t>(INT32_MIN) && value <= static_cast<int64_t>(INT32_MAX);
    }
    else if constexpr (std::is_same_v<T, int64_t>)
    {
        // value is same as the input type, so it can't be out of range
        return true;
    }

    JXC_UNREACHABLE("Unreachable");
    return false;
}

// determines if an unsigned integer would fit in a smaller unsigned integer type
template<typename T>
constexpr bool unsigned_integer_value_fits_in_type(uint64_t value)
{
    static_assert(std::is_integral_v<T>, "unsigned_integer_value_fits_in_type requires an integer type");
    static_assert(std::is_unsigned_v<T>, "unsigned_integer_value_fits_in_type requires an unsigned integer type");

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        return value <= static_cast<uint64_t>(UINT8_MAX);
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        return value <= static_cast<uint64_t>(UINT16_MAX);
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        return value <= static_cast<uint64_t>(UINT32_MAX);
    }
    else if constexpr (std::is_same_v<T, uint64_t>)
    {
        // value is same as the input type, so it can't be out of range
        return true;
    }

    JXC_UNREACHABLE("Unreachable");
    return false;
}

JXC_END_NAMESPACE(detail)


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


inline int32_t char_to_int(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    JXC_ASSERTF(is_decimal_digit(ch), "char_to_int requires a decimal digit char, got {}", detail::debug_char_repr(ch));
    return 0;
}


inline int32_t char_to_int_unchecked(char ch) { return ch - '0'; }


JXC_EXPORT bool string_is_number_base_10(std::string_view value);


JXC_EXPORT bool string_to_double(std::string_view value, double& out_value);


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
inline bool string_to_number_decimal(char sign_char, std::string_view value, T& out_result, bool* out_overflow = nullptr)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "string_to_number_decimal requires a numeric type");

    // strip leading zeroes
    while (value.size() >= 2 && value[0] == '0' && value[1] == '0')
    {
        value = value.substr(1);
    }

    // empty value
    if (value.size() == 0)
    {
        out_result = 0;
        return false;
    }

    JXC_DEBUG_ASSERTF(string_is_number_base_10(value), "Invalid decimal number {}", detail::debug_string_repr(value));

    // handle unsigned integers with a negative sign
    if constexpr (std::is_unsigned_v<T>)
    {
        if (sign_char == '-')
        {
            out_result = 0;

            // negative zero is fine
            if (value == "0")
            {
                return true;
            }

            // no other negative values are valid
            if (out_overflow != nullptr)
            {
                *out_overflow = true;
            }
            return false;
        }
    }

    // fast-path for single-character decimal numbers
    if (value.size() == 1)
    {
        if constexpr (std::is_unsigned_v<T>)
        {
            if (sign_char == '-')
            {
                out_result = 0;
            }
        }

        if (is_decimal_digit(value[0]))
        {
            out_result = static_cast<T>(char_to_int(value[0])) * detail::sign_char_to_multiplier<T>(sign_char);
            return true;
        }
        return false;
    }

    if constexpr (std::is_floating_point_v<T>)
    {
        // use the fast_float library to convert floating point values
        if (!string_to_float<T>(value, out_result))
        {
            return false;
        }

        // can use this instead if you want to get rid of fast_float (it's about 4x slower though)
        //out_result = static_cast<T>(strtold(value.data(), nullptr));

        out_result *= detail::sign_char_to_multiplier<T>(sign_char);
        return true;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        // max value for uint64 is 20 digits long, so if we have more than that, we can't store it in any standard integer type
        if (value.size() > 20)
        {
            if (out_overflow != nullptr)
            {
                *out_overflow = true;
            }
            out_result = 0;
            return false;
        }

        // check for invalid digits
        for (size_t i = 0; i < value.size(); i++)
        {
            if (!is_decimal_digit(value[i]))
            {
                out_result = 0;
                return false;
            }
        }

        // if the value is signed and negative, check if it's greater than or equal to the minimum value for this int type
        if constexpr (std::is_signed_v<T>)
        {
            if (sign_char == '-' && !detail::value_gte_int_min<T>(value))
            {
                if (out_overflow != nullptr)
                {
                    *out_overflow = true;
                }
                out_result = 0;
                return false;
            }
        }

        // check if the value is less than or equal to the maximum value for this int type
        if (sign_char == '+' && !detail::value_lte_int_max<T>(value))
        {
            if (out_overflow != nullptr)
            {
                *out_overflow = true;
            }
            out_result = 0;
            return false;
        }

        if constexpr (std::is_unsigned_v<T>)
        {
            out_result = static_cast<T>(strtoull(value.data(), nullptr, 10));
        }
        else
        {
            out_result = static_cast<T>(strtoll(value.data(), nullptr, 10)) * detail::sign_char_to_multiplier<T>(sign_char);

            // edge case - if the value is exactly INT64_MIN, then we actually end up with INT64_MIN+1,
            // because we can't represent abs(INT64_MIN) before factoring in the multiplier.
            if constexpr (std::is_same_v<T, int64_t>)
            {
                if (sign_char == '-' && out_result == (INT64_MIN + 1) && value == "9223372036854775808")
                {
                    out_result = INT64_MIN;
                }
            }
        }

        return true;
    }

    JXC_UNREACHABLE("unreachable: string_to_number_decimal");
    return false;
}


template<typename T>
inline bool string_to_number_hex(char sign_char, std::string_view value, T& out_result)
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

    out_result = static_cast<T>(strtoll(value.data(), nullptr, 16)) * detail::sign_char_to_multiplier<T>(sign_char);
    return true;
}


template<typename T>
inline bool string_to_number_octal(char sign_char, std::string_view value, T& out_result)
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
    out_result = static_cast<T>(strtol(value.data(), nullptr, 8)) * detail::sign_char_to_multiplier<T>(sign_char);
    return true;
}


template<typename T>
inline bool string_to_number_binary(char sign_char, std::string_view value, T& out_result)
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
    out_result = static_cast<T>(strtol(value.data(), nullptr, 2)) * detail::sign_char_to_multiplier<T>(sign_char);
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


struct JXC_EXPORT NumberTokenSplitResult
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


JXC_EXPORT bool split_number_token_value(const Token& number_token, NumberTokenSplitResult& out_result, ErrorInfo& out_error);


template<typename T>
bool parse_number(const Token& tok, T& out_value, const NumberTokenSplitResult& number, ErrorInfo& out_error)
{
    // test for unsigned negative values
    if constexpr (std::is_unsigned_v<T>)
    {
        if (number.sign == '-' && number.value != "0")
        {
            out_error = ErrorInfo{ "parse_number got an unsigned type, but the number is negative", tok.start_idx, tok.end_idx };
            return false;
        }
    }

    // handle float literal types
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
            if (!string_to_number_hex<T>(number.sign, number.value, out_value))
            {
                out_error = ErrorInfo{ jxc::format("Value {} is not a valid hex literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
                return false;
            }
            return true;

        case 'b':
        case 'B':
            if (!string_to_number_binary<T>(number.sign, number.value, out_value))
            {
                out_error = ErrorInfo{ jxc::format("Value {} is not a valid binary literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
                return false;
            }
            return true;

        case 'o':
        case 'O':
            if (!string_to_number_octal<T>(number.sign, number.value, out_value))
            {
                out_error = ErrorInfo{ jxc::format("Value {} is not a valid octal literal", detail::debug_string_repr(number.value)), tok.start_idx, tok.end_idx };
                return false;
            }
            return true;

        default:
            break;
        }

        return false;
    }

    bool overflow_error = false;
    if (!string_to_number_decimal<T>(number.sign, number.value, out_value, &overflow_error))
    {
        if (overflow_error)
        {
            out_error = ErrorInfo{ jxc::format("Value {} is too large for integer type {}",
                detail::debug_string_repr(number.value), detail::get_type_name<T>()),
                tok.start_idx, tok.end_idx };
        }
        else
        {
            out_error = ErrorInfo{ jxc::format("Value {} is not a valid decimal literal for type {}",
                detail::debug_string_repr(number.value), detail::get_type_name<T>()),
                tok.start_idx, tok.end_idx };
        }
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
bool parse_number_simple(const Token& tok, NumberType& out_value, ErrorInfo& out_error, SuffixStringType* out_suffix = nullptr)
{
    if (tok.type != TokenType::Number)
    {
        out_error = ErrorInfo(jxc::format("Expected Number token, got {}", token_type_to_string(tok.type)), tok.start_idx, tok.end_idx);
        return false;
    }

    NumberTokenSplitResult number;
    if (!split_number_token_value(tok, number, out_error))
    {
        return false;
    }

    if (!parse_number<NumberType>(tok, out_value, number, out_error))
    {
        return false;
    }

    if (out_suffix != nullptr)
    {
        *out_suffix = SuffixStringType(number.suffix);
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
JXC_EXPORT bool string_token_to_value(const Token& string_token, std::string_view& out_view, bool& out_is_raw_string, ErrorInfo& out_error);

// Checks if a string contains any backslash characters
JXC_EXPORT bool string_has_escape_chars(std::string_view string_value);

// Returns the required buffer size to store a raw string, based on the string's value (see string_token_to_value())
inline size_t get_raw_string_required_buffer_size(std::string_view string_value) { return string_value.size(); }

// Returns the required buffer size to store a string, based on the string's value (see string_token_to_value())
// This scans the string and checks escape characters for how many bytes they require.
// Note that for strings with a large number of unicode escapes, this can overestimate a bit.
JXC_EXPORT size_t get_string_required_buffer_size(std::string_view string_value);

// Returns the required buffer size to store a string, based on the string's value (see string_token_to_value())
inline size_t get_string_required_buffer_size(std::string_view string_value, bool is_raw_string)
{
    return is_raw_string ? get_raw_string_required_buffer_size(string_value) : get_string_required_buffer_size(string_value);
}

// Returns the required buffer size to store a string.
JXC_EXPORT bool get_string_token_required_buffer_size(const Token& string_token, size_t& out_buffer_size, ErrorInfo& out_error);

// Scans a string value and parses its contents for escape characters, storing them in the output buffer.
// Use string_token_to_value() to convert a token to a string value, and get_string_required_buffer_size() to compute the buffer size needed.
// Note that this does not need to be called for raw strings.
JXC_EXPORT bool parse_string_escapes_to_buffer(std::string_view string_value, size_t string_token_start_idx, size_t string_token_end_idx, char* out_string_buffer,
    size_t string_buffer_size, size_t& out_num_chars_written, ErrorInfo& out_error);

// Parses a string token to a string buffer. Use get_string_token_required_buffer_size() to compute the size of the output buffer.
JXC_EXPORT bool parse_string_token_to_buffer(const Token& string_token, char* out_buffer, size_t buffer_size, size_t& out_num_chars_written, ErrorInfo& out_error);

// Parses a string token to a resizable character buffer (eg. std::string or std::vector<char>)
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
JXC_EXPORT size_t get_byte_buffer_required_size(const char* bytes_token_str, size_t bytes_token_str_len);

JXC_EXPORT bool parse_bytes_token(const Token& bytes_token, uint8_t* out_data_buffer, size_t out_data_buffer_size, size_t& out_num_bytes_written, ErrorInfo& out_error);

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
JXC_EXPORT bool datetime_token_is_date(const Token& datetime_token);

// Checks if the given datetime token includes both date and time data
JXC_EXPORT bool datetime_token_is_datetime(const Token& datetime_token);

// Parses a date token into a Date value. This will return an error if the token includes time data.
JXC_EXPORT bool parse_date_token(const Token& datetime_token, Date& out_date, ErrorInfo& out_error);

// Parses a date token into a DateTime value.
// If require_time_data is true, this will return an error if the token does not include time info.
// If require_time_data is false and the token does not include time data, out_datetime will have a time of 00:00:00Z.
JXC_EXPORT bool parse_datetime_token(const Token& datetime_token, DateTime& out_datetime, ErrorInfo& out_error, bool require_time_data = false);

JXC_END_NAMESPACE(util)

JXC_END_NAMESPACE(jxc)
