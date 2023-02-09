#pragma once
#include "jxc/jxc_core.h"
#include "jxc/jxc_util.h"


JXC_BEGIN_NAMESPACE(jxc)

struct Lexer
{
    const uint8_t* start = nullptr;
    const uint8_t* token_start = nullptr;
    const uint8_t* current = nullptr;
    const uint8_t* marker = nullptr;
    const uint8_t* limit = nullptr;
    const uint8_t* ctxmarker = nullptr;
    size_t line = 1;

    // paren `()`, bracket `[]`, and brace `{}` sanity checks
    int64_t expr_paren_depth = 0;
    int64_t expr_bracket_depth = 0;
    int64_t expr_brace_depth = 0;

    // need to track these to avoid using the expression parsing stuff inside type annotation angle brackets
    int64_t angle_bracket_depth = 0;

    Lexer() = default;

    Lexer(const char* data, size_t len)
        : start(reinterpret_cast<const uint8_t*>(data))
        , token_start(start)
        , current(start)
        , marker(start)
        , limit(start + len)
    {
    }

    Lexer(const uint8_t* data, const uint8_t* limit)
        : start(data)
        , token_start(data)
        , current(data)
        , marker(data)
        , limit(limit)
    {
    }

    TokenType expr_next(ErrorInfo& out_error, size_t& out_start_idx, size_t& out_end_idx, std::string_view& out_token_value, std::string_view& out_tag, bool expr_only_mode = false);

    TokenType next(ErrorInfo& out_error, size_t& out_start_idx, size_t& out_end_idx, std::string_view& out_token_value, std::string_view& out_tag);

    inline bool next(Token& out_token, ErrorInfo& out_error)
    {
        std::string_view tok_value;
        std::string_view tok_tag;
        out_token.type = next(out_error, out_token.start_idx, out_token.end_idx, tok_value, tok_tag);
        out_token.value = FlexString::make_view(tok_value);
        out_token.tag = FlexString::make_view(tok_tag);
        return out_token.type != TokenType::EndOfStream && !out_error.is_err;
    }

private:
    bool scan_comment(size_t comment_token_len, std::string_view& out_comment);
    bool scan_hex_escape(std::string& out_error_message);
    bool scan_utf16_escape(std::string& out_error_message);
    bool scan_utf32_escape(std::string& out_error_message);
    bool scan_string(std::string& out_error_message, uint8_t quote_char, std::string_view& out_string);
    bool scan_raw_string(std::string& out_error_message, uint8_t quote_char, std::string_view& out_string, std::string_view& out_delim);
    bool scan_base64_string(std::string& out_error_message, std::string_view& out_string_token);
    bool scan_datetime_string(std::string& out_error_message, std::string_view& out_datetime_token);

public:
    inline void get_token_pos(size_t& out_start, size_t& out_end)
    {
        out_start = token_start - start;
        out_end = current - start;
    }

    inline std::string_view get_view() const
    {
        return std::string_view{ reinterpret_cast<const char*>(start), (size_t)(limit - start) };
    }
};

JXC_BEGIN_NAMESPACE(detail)

class BaseHelperLexer
{
protected:
    Lexer lex;
    mutable ErrorInfo lex_error;
    std::string_view buffer;
    std::string_view tok_value;
    std::string_view tok_tag;

public:
    BaseHelperLexer(std::string_view buf)
        : lex(buf.data(), buf.size())
        , buffer(buf)
    {
    }

    inline bool has_error() const
    {
        return lex_error.is_err;
    }

    inline const ErrorInfo& get_error() const
    {
        return lex_error;
    }

    inline bool set_parse_error(Token& inout_token, std::string&& msg)
    {
        lex_error = ErrorInfo(std::forward<std::string>(msg), inout_token.start_idx, inout_token.end_idx);
        inout_token.type = TokenType::Invalid;
        return false;
    }

    inline std::string get_error_message() const
    {
        if (lex_error.is_err)
        {
            lex_error.get_line_and_col_from_buffer(buffer);
            return lex_error.to_string(buffer);
        }
        return std::string{};
    }
};

JXC_END_NAMESPACE(detail)


// helper class lexing any JXC string into tokens
class TokenLexer : public detail::BaseHelperLexer
{
public:
    using detail::BaseHelperLexer::BaseHelperLexer; // inherit constructors
    inline bool next(Token& out_token) { return lex.next(out_token, lex_error); }
};


// helper class for parsing an annotation into tokens
class AnnotationLexer : public detail::BaseHelperLexer
{
    size_t num_tokens = 0;
    bool got_complete_angle_bracket_set = false;
    TokenType last_token_type = TokenType::Invalid;
    TokenType req_next_token_type = TokenType::Invalid;
public:
    using detail::BaseHelperLexer::BaseHelperLexer; // inherit constructors
    bool next(Token& out_token);
};


// helper class for parsing an expression into tokens
class ExpressionLexer : public detail::BaseHelperLexer
{
public:
    using detail::BaseHelperLexer::BaseHelperLexer; // inherit constructors
    bool next(Token& out_token);
};


JXC_END_NAMESPACE(jxc)

