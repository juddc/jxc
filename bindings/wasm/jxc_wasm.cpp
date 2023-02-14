#include <emscripten/bind.h>
#include <optional>
#include "jxc/jxc.h"


namespace em = emscripten;


static constexpr int64_t js_min_safe_integer = -9007199254740991;
static constexpr int64_t js_max_safe_integer = 9007199254740991;

static constexpr double js_min_safe_float = 5e-324;
static constexpr double js_max_safe_float = 1.7976931348623157e+308;


template<typename T>
inline std::optional<double> js_cast_number(T value)
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "js_cast_number requires a numeric type");

    if constexpr (std::is_unsigned_v<T> && sizeof(value) == sizeof(size_t))
    {
        if (value == jxc::invalid_idx)
        {
            return std::nullopt;
        }
    }

    if constexpr (std::is_integral_v<T>)
    {
        if constexpr (std::is_unsigned_v<T>)
        {
            const uint64_t result = static_cast<uint64_t>(value);
            if (result <= (uint64_t)js_max_safe_integer)
            {
                return static_cast<double>(result);
            }
            else
            {
                return std::nullopt;
            }
        }
        else
        {
            const int64_t result = static_cast<int64_t>(value);
            if (result >= js_min_safe_integer && result <= js_max_safe_integer)
            {
                return static_cast<double>(result);
            }
            else
            {
                return std::nullopt;
            }
        }
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        const double result = static_cast<double>(value);
        if (result >= js_min_safe_float && result <= js_max_safe_float)
        {
            return result;
        }
        return std::nullopt;
    }
    else
    {
        return std::nullopt;
    }
}


class JsLexer
{
private:
    std::string buffer;
    jxc::Lexer lexer;
    jxc::ErrorInfo current_error;
    jxc::Token current_token;

public:
    JsLexer(std::string in_buffer)
        : buffer(in_buffer)
        , lexer(buffer.data(), buffer.size())
    {
    }

    bool next()
    {
        return lexer.next(current_token, current_error);
    }

    jxc::TokenType token_type() const
    {
        return current_token.type;
    }

    std::string token_value() const
    {
        return std::string(current_token.value.as_view());
    }

    double token_start_idx() const
    {
        return js_cast_number(current_token.start_idx).value_or(-1.0);
    }

    double token_end_idx() const
    {
        return js_cast_number(current_token.end_idx).value_or(-1.0);
    }

    bool has_error() const
    {
        return current_error.is_err;
    }

    std::string get_error() const
    {
        if (!current_error.is_err) { return std::string(); }
        jxc::ErrorInfo err = current_error;
        err.get_line_and_col_from_buffer(buffer);
        return err.to_string(buffer);
    }
};


template<typename T>
struct JsParsedValue
{
    T value;
    bool is_valid = true;
    std::string error_message;

    JsParsedValue() : is_valid(false) {}
    JsParsedValue(T value) : value(value), is_valid(true) {}

    static JsParsedValue make_error(const std::string& err)
    {
        JsParsedValue result{};
        result.value = T{};
        result.is_valid = false;
        result.error_message = err;
        return result;
    }
};


template<typename T>
void bind_parsed_value(const char* name)
{
    em::value_object<JsParsedValue<T>>(name)
        .field("value", &JsParsedValue<T>::value)
        .field("isValid", &JsParsedValue<T>::is_valid)
        .field("errorMessage", &JsParsedValue<T>::error_message)
    ;
}


struct JsNumberTokenSplitResult
{
    char sign = '\0';
    std::string prefix;
    std::string value;
    double exponent = 0.0;
    std::string suffix;
};


JsParsedValue<JsNumberTokenSplitResult> js_split_number_token_value(jxc::TokenType tok_type, std::string tok_value, size_t tok_start_idx, size_t tok_end_idx)
{
    JsParsedValue<JsNumberTokenSplitResult> result;
    jxc::Token tok(tok_type, tok_start_idx, tok_end_idx, jxc::FlexString::make_view(tok_value));
    jxc::util::NumberTokenSplitResult split;
    jxc::ErrorInfo err;
    if (jxc::util::split_number_token_value(tok, split, err))
    {
        result.is_valid = true;
        result.value.sign = split.sign;
        result.value.prefix = std::string(split.prefix);
        result.value.value = std::string(split.value);
        result.value.exponent = js_cast_number(split.exponent).value_or(0.0);
        result.value.suffix = std::string(split.suffix);
    }
    else
    {
        result.is_valid = false;
        result.error_message = err.to_string();
    }
    return result;
}


struct NumberWithSuffix
{
    double value = 0.0;
    std::string suffix;

    NumberWithSuffix() {}
    NumberWithSuffix(double value) : value(value) {}
    NumberWithSuffix(double value, std::string_view suffix) : value(value), suffix(std::string(suffix)) {}
};


JsParsedValue<NumberWithSuffix> js_parse_number_token(jxc::TokenType tok_type, std::string tok_value, size_t tok_start_idx, size_t tok_end_idx)
{
    JsParsedValue<NumberWithSuffix> result;
    result.is_valid = jxc::util::parse_number_simple<double, std::string>(tok_value, result.value.value, &result.error_message, &result.value.suffix);
    return result;
}


JsParsedValue<NumberWithSuffix> js_parse_number_object_key(jxc::TokenType tok_type, std::string tok_value, size_t tok_start_idx, size_t tok_end_idx)
{
    JsParsedValue<NumberWithSuffix> result;
    jxc::Token tok(tok_type, tok_start_idx, tok_end_idx, jxc::FlexString::make_view(tok_value));
    int64_t int_key = 0;
    jxc::ErrorInfo err;
    if (jxc::util::parse_number_object_key<int64_t, std::string>(tok, int_key, err, &result.value.suffix))
    {
        result.is_valid = true;
        result.value.value = static_cast<double>(int_key);
    }
    else
    {
        result.is_valid = false;
        result.error_message = err.to_string();
    }
    return result;
}


JsParsedValue<std::string> js_parse_string_token(jxc::TokenType tok_type, std::string tok_value, size_t tok_start_idx, size_t tok_end_idx)
{
    JsParsedValue<std::string> result;
    jxc::Token tok(tok_type, tok_start_idx, tok_end_idx, jxc::FlexString::make_view(tok_value));
    jxc::ErrorInfo err;
    if (jxc::util::parse_string_token(tok, result.value, err))
    {
        result.is_valid = true;
    }
    else
    {
        result.is_valid = false;
        result.error_message = err.to_string();
    }
    return result;
}


JsParsedValue<std::vector<uint8_t>> js_parse_bytes_token(jxc::TokenType tok_type, std::string tok_value, size_t tok_start_idx, size_t tok_end_idx)
{
    JsParsedValue<std::vector<uint8_t>> result;
    jxc::Token tok(tok_type, tok_start_idx, tok_end_idx, jxc::FlexString::make_view(tok_value));
    jxc::ErrorInfo err;
    if (jxc::util::parse_bytes_token(tok, result.value, err))
    {
        result.is_valid = true;
    }
    else
    {
        result.is_valid = false;
        result.error_message = err.to_string();
    }
    return result;
}


std::string js_token_type_to_string(jxc::TokenType type)
{
    return std::string(jxc::token_type_to_string(type));
}


EMSCRIPTEN_BINDINGS(jxc)
{
    em::enum_<jxc::TokenType>("TokenType")
        .value("Invalid", jxc::TokenType::Invalid)
        .value("Comment", jxc::TokenType::Comment)
        .value("Identifier", jxc::TokenType::Identifier)
        .value("True", jxc::TokenType::True)
        .value("False", jxc::TokenType::False)
        .value("Null", jxc::TokenType::Null)
        .value("Number", jxc::TokenType::Number)
        .value("String", jxc::TokenType::String)
        .value("ByteString", jxc::TokenType::ByteString)
        .value("DateTime", jxc::TokenType::DateTime)
        .value("Colon", jxc::TokenType::Colon)
        .value("Equals", jxc::TokenType::Equals)
        .value("Comma", jxc::TokenType::Comma)
        .value("Period", jxc::TokenType::Period)
        .value("BraceOpen", jxc::TokenType::BraceOpen)
        .value("BraceClose", jxc::TokenType::BraceClose)
        .value("SquareBracketOpen", jxc::TokenType::SquareBracketOpen)
        .value("SquareBracketClose", jxc::TokenType::SquareBracketClose)
        .value("AngleBracketOpen", jxc::TokenType::AngleBracketOpen)
        .value("AngleBracketClose", jxc::TokenType::AngleBracketClose)
        .value("ParenOpen", jxc::TokenType::ParenOpen)
        .value("ParenClose", jxc::TokenType::ParenClose)
        .value("ExclamationPoint", jxc::TokenType::ExclamationPoint)
        .value("Asterisk", jxc::TokenType::Asterisk)
        .value("QuestionMark", jxc::TokenType::QuestionMark)
        .value("AtSymbol", jxc::TokenType::AtSymbol)
        .value("Pipe", jxc::TokenType::Pipe)
        .value("Ampersand", jxc::TokenType::Ampersand)
        .value("Percent", jxc::TokenType::Percent)
        .value("Semicolon", jxc::TokenType::Semicolon)
        .value("Plus", jxc::TokenType::Plus)
        .value("Minus", jxc::TokenType::Minus)
        .value("Slash", jxc::TokenType::Slash)
        .value("Backslash", jxc::TokenType::Backslash)
        .value("Caret", jxc::TokenType::Caret)
        .value("Tilde", jxc::TokenType::Tilde)
        .value("Backtick", jxc::TokenType::Backtick)
        .value("LineBreak", jxc::TokenType::LineBreak)
        .value("EndOfStream", jxc::TokenType::EndOfStream)
    ;

    em::function("tokenTypeToString", &js_token_type_to_string);

    em::class_<JsLexer>("Lexer")
        .constructor<std::string>()
        .function("next", &JsLexer::next)
        .property("tokenType", &JsLexer::token_type)
        .property("tokenValue", &JsLexer::token_value)
        .property("tokenStartIdx", &JsLexer::token_start_idx)
        .property("tokenEndIdx", &JsLexer::token_end_idx)
        .function("hasError", &JsLexer::has_error)
        .function("getError", &JsLexer::get_error)
    ;

    em::value_object<NumberWithSuffix>("NumberWithSuffix")
        .field("value", &NumberWithSuffix::value)
        .field("suffix", &NumberWithSuffix::suffix)
    ;

    em::value_object<JsNumberTokenSplitResult>("NumberTokenSplitResult")
        .field("sign", &JsNumberTokenSplitResult::sign)
        .field("prefix", &JsNumberTokenSplitResult::prefix)
        .field("value", &JsNumberTokenSplitResult::value)
        .field("exponent", &JsNumberTokenSplitResult::exponent)
        .field("suffix", &JsNumberTokenSplitResult::suffix)
    ;

    em::register_vector<uint8_t>("Bytes");

    bind_parsed_value<JsNumberTokenSplitResult>("ParsedNumberTokenSplitResult");
    bind_parsed_value<std::vector<uint8_t>>("ParsedBytes");
    bind_parsed_value<NumberWithSuffix>("ParsedNumberWithSuffix");
    bind_parsed_value<std::string>("ParsedString");

    em::function("parseStringToken", &js_parse_string_token);
    em::function("parseBytesToken", &js_parse_bytes_token);
    em::function("parseNumberToken", &js_parse_number_token);
    em::function("parseNumberObjectKey", &js_parse_number_object_key);
    em::function("splitNumberTokenValue", &js_split_number_token_value);

}
