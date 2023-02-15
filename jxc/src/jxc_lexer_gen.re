#include "jxc/jxc_lexer.h"


#define YYCTYPE  uint8_t
#define YYCURSOR this->current
#define YYMARKER this->marker


/*!re2c
    re2c:define:YYCURSOR = this->current;
    re2c:define:YYMARKER = this->marker;
    re2c:define:YYLIMIT = this->limit;
    re2c:define:YYCTXMARKER = this->ctxmarker;
    re2c:encoding:utf8 = 1;
    re2c:yyfill:enable = 0;

    // spaces only
    spaces = [ \t]+;

    // linebreaks only
    linebreak = [\r\n]+;

    // any amount of spaces, tabs, or line breaks
    whitespace = [ \t\r\n]+;

    zero = "0";
    digit = [0-9];
    digit_1_9 = [1-9];
    hex_digit = [0-9a-fA-F];
    bin_digit = [01];
    oct_digit = [0-7];
    base64_digit = [A-Za-z0-9+/=];
    decimal_pt = ".";
    minus = "-";
    plus = "+";
    integer = zero | (digit_1_9 digit*);
    frac = decimal_pt digit+;
    exponent = ("e" | "E") (minus | plus)? digit+;
    number_type_suffix = "_"? [a-zA-Z%] [a-zA-Z0-9_%]{0,15};
    hex_number_type_suffix = "_" [a-zA-Z%] [a-zA-Z0-9_%]{0,15};

    identifier = [a-zA-Z_$][a-zA-Z0-9_$]*;

    quote = ['"];

    str_prefix_raw = "r";

    str_base64 = "b64\"(" | "b64'(" | "b64\"" | "b64'";

    iso8601_date = ("+" | "-")? [0-9]{4,5} "-" [0-9]{2} "-" [0-9]{2};
    iso8601_time = [0-9]{2} ":" [0-9]{2} (":" [0-9]{2})?;
    iso8601_frac = "." [0-9]{1,12};
    iso8601_tz = "Z" | ([+-] [0-9]{2} ":" [0-9]{2});
    iso8601_value = iso8601_date | (iso8601_date "T" iso8601_time iso8601_frac? iso8601_tz?);
    str_datetime = ("dt\"" iso8601_value "\"") | ("dt'" iso8601_value "'");

    str_datetime_prefix = "dt\"" | "dt'";

    object_key_identifier = [a-zA-Z_$*][a-zA-Z0-9_$*]*;
    object_key_sep = ".";
    object_key = object_key_identifier (object_key_sep object_key_identifier)*;

    unsigned_number_value = (
          ("0x" hex_digit+ hex_number_type_suffix?)
        | ("0b" bin_digit+ number_type_suffix?)
        | ("0o" oct_digit+ number_type_suffix?)
        | (integer number_type_suffix?)
        | (integer exponent number_type_suffix?)
        | (integer frac exponent? number_type_suffix?)
    );

    number_value = (minus | plus)? unsigned_number_value;
*/


JXC_BEGIN_NAMESPACE(jxc)

TokenType Lexer::expr_next(ErrorInfo& out_error, size_t& out_start_idx, size_t& out_end_idx, std::string_view& out_token_value, std::string_view& out_string_delim, bool expr_only_mode)
{
    JXC_DEBUG_ASSERT(expr_only_mode || expr_paren_depth > 0);
    JXC_DEBUG_ASSERT(angle_bracket_depth == 0);

    auto set_token = [&]()
    {
        this->get_token_pos(out_start_idx, out_end_idx);
        out_token_value = std::string_view{ (char*)this->token_start, static_cast<size_t>(this->current - this->token_start) };
    };

    auto set_error_msg = [&](std::string&& err)
    {
        this->get_token_pos(out_start_idx, out_end_idx);
        out_error = ErrorInfo(std::move(err), out_start_idx, out_end_idx);
    };

    auto set_error = [&]()
    {
        // assumes out_error.message was already set
        JXC_DEBUG_ASSERT(out_error.message.size() > 0);
        this->get_token_pos(out_start_idx, out_end_idx);
        out_error.is_err = true;
        out_error.buffer_start_idx = out_start_idx;
        out_error.buffer_end_idx = out_end_idx;
    };

expr_start:
    if (this->current > this->limit)
    {
        goto expr_end_of_stream;
    }
    this->token_start = this->current;

    /*!local:re2c

    // expressions allow a limited subset of JXC syntax

    // expression start/end
    "("                             { set_token(); ++expr_paren_depth; return TokenType::ParenOpen; }
    ")"                             { set_token(); --expr_paren_depth; if (expr_paren_depth < 0) { set_error_msg("Unexpected symbol `)` in expression"); return TokenType::Invalid; } else { return TokenType::ParenClose; } }

    // allow square brackets, but enforce balanced brackets
    "["                             { set_token(); ++expr_bracket_depth; return TokenType::SquareBracketOpen; }
    "]"                             { set_token(); --expr_bracket_depth; if (expr_bracket_depth < 0) { set_error_msg("Unexpected symbol `]` in expression"); return TokenType::Invalid; } else { return TokenType::SquareBracketClose; } }

    // allow curly braces, but enforce balanced braces
    "{"                             { set_token(); ++expr_brace_depth; return TokenType::BraceOpen; }
    "}"                             { set_token(); --expr_brace_depth; if (expr_brace_depth < 0) { set_error_msg("Unexpected symbol `}` in expression"); return TokenType::Invalid; } else { return TokenType::BraceClose; } }

    // allow comments
    "#"                             { scan_comment(1, out_token_value); get_token_pos(out_start_idx, out_end_idx); return TokenType::Comment; }

    // NB. we match *unsigned* numbers only here to avoid operator mangling issues
    unsigned_number_value           { set_token(); return TokenType::Number; }

    // literal constants
    "true"                          { set_token(); return TokenType::True; }
    "false"                         { set_token(); return TokenType::False; }
    "null"                          { set_token(); return TokenType::Null; }
    "nan"                           { set_token(); return TokenType::Number; }
    "inf"                           { set_token(); return TokenType::Number; }

    // symbols that can be used as an operator/syntax token inside expressions
    ","                             { set_token(); return TokenType::Comma; }
    ":"                             { set_token(); return TokenType::Colon; }
    "@"                             { set_token(); return TokenType::AtSymbol; }
    "|"                             { set_token(); return TokenType::Pipe; }
    "&"                             { set_token(); return TokenType::Ampersand; }
    "!"                             { set_token(); return TokenType::ExclamationPoint; }
    "="                             { set_token(); return TokenType::Equals; }
    "+"                             { set_token(); return TokenType::Plus; }
    "-"                             { set_token(); return TokenType::Minus; }
    "*"                             { set_token(); return TokenType::Asterisk; }
    "/"                             { set_token(); return TokenType::Slash; }
    "\\"                            { set_token(); return TokenType::Backslash; }
    "%"                             { set_token(); return TokenType::Percent; }
    "^"                             { set_token(); return TokenType::Caret; }
    "."                             { set_token(); return TokenType::Period; }
    "?"                             { set_token(); return TokenType::QuestionMark; }
    "~"                             { set_token(); return TokenType::Tilde; }
    "<"                             { set_token(); return TokenType::AngleBracketOpen; }
    ">"                             { set_token(); return TokenType::AngleBracketClose; }
    "`"                             { set_token(); return TokenType::Backtick; }
    ";"                             { set_token(); return TokenType::Semicolon; }

    // string
    str_prefix_raw quote            { if (scan_raw_string(out_error.message, this->current[-1], out_token_value, out_string_delim)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::String; } else { set_error(); return TokenType::Invalid; } }
    str_base64                      { if (scan_base64_string(out_error.message, out_token_value)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::ByteString; } else { set_error(); return TokenType::Invalid; } }
    str_datetime                    { set_token(); return TokenType::DateTime; }
    str_datetime_prefix             { if (scan_datetime_string(out_error.message, out_token_value)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::DateTime; } else { set_error(); return TokenType::Invalid; } }
    quote                           { if (scan_string(out_error.message, this->current[-1], out_token_value)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::String; } else { set_error(); return TokenType::Invalid; } }

    // identifiers
    identifier                      { set_token(); return TokenType::Identifier; }

    spaces                          { goto expr_start; }
    linebreak (spaces linebreak)*   { set_token(); return TokenType::LineBreak; }

    * {
        if (this->current >= this->limit)
        {
            goto expr_end_of_stream;
        }
        else
        {
            set_error_msg("Invalid syntax while parsing expression");
            return TokenType::Invalid;
        }
    }

    */

expr_end_of_stream:
    if (expr_only_mode)
    {
        set_token();
        return TokenType::EndOfStream;
    }
    else
    {
        set_error_msg("Unexpected end of stream while parsing expression");
        return TokenType::Invalid;
    }
}


TokenType Lexer::next(ErrorInfo& out_error, size_t& out_start_idx, size_t& out_end_idx, std::string_view& out_token_value, std::string_view& out_string_delim)
{
    out_token_value = {};
    out_string_delim = {};

    if (angle_bracket_depth == 0 && expr_paren_depth > 0)
    {
        return expr_next(out_error, out_start_idx, out_end_idx, out_token_value, out_string_delim);
    }

    auto set_token = [&]()
    {
        this->get_token_pos(out_start_idx, out_end_idx);
        out_token_value = std::string_view{ (char*)this->token_start, static_cast<size_t>(this->current - this->token_start) };
    };

    auto set_error_msg = [&](std::string&& err)
    {
        this->get_token_pos(out_start_idx, out_end_idx);
        out_error = ErrorInfo(std::move(err), out_start_idx, out_end_idx);
    };

    auto set_error = [&]()
    {
        // assumes out_error.message was already set
        JXC_DEBUG_ASSERT(out_error.message.size() > 0);
        this->get_token_pos(out_start_idx, out_end_idx);
        out_error.is_err = true;
        out_error.buffer_start_idx = out_start_idx;
        out_error.buffer_end_idx = out_end_idx;
    };

regular:
    if (this->current > this->limit)
    {
        return TokenType::EndOfStream;
    }
    this->token_start = this->current;

    /*!local:re2c

    ":"                             { set_token(); return TokenType::Colon; }
    "="                             { set_token(); return TokenType::Equals; }
    ","                             { set_token(); return TokenType::Comma; }
    "."                             { set_token(); return TokenType::Period; }
    "{"                             { set_token(); return TokenType::BraceOpen; }
    "}"                             { set_token(); return TokenType::BraceClose; }
    "["                             { set_token(); return TokenType::SquareBracketOpen; }
    "]"                             { set_token(); return TokenType::SquareBracketClose; }
    "<"                             { set_token(); ++angle_bracket_depth; return TokenType::AngleBracketOpen; }
    ">"                             { set_token(); --angle_bracket_depth; if (angle_bracket_depth < 0) { set_error_msg("Unmatched angle brackets"); return TokenType::Invalid; } else { return TokenType::AngleBracketClose; } }

    // tokens usable inside annotations
    "!"                             { set_token(); return TokenType::ExclamationPoint; }
    "*"                             { set_token(); return TokenType::Asterisk; }
    "?"                             { set_token(); return TokenType::QuestionMark; }
    "|"                             { set_token(); return TokenType::Pipe; }
    "&"                             { set_token(); return TokenType::Ampersand; }

    // comments
    "#"                             { scan_comment(1, out_token_value); get_token_pos(out_start_idx, out_end_idx); return TokenType::Comment; }

    // literal constants
    "true"                          { set_token(); return TokenType::True; }
    "false"                         { set_token(); return TokenType::False; }
    "null"                          { set_token(); return TokenType::Null; }

    // expressions
    "("                             { set_token(); ++expr_paren_depth; return TokenType::ParenOpen; }
    ")"                             { set_token(); --expr_paren_depth; if (expr_paren_depth < 0) { set_error_msg("Unmatched parentheses"); return TokenType::Invalid; } else { return TokenType::ParenClose; } }

    // string
    str_prefix_raw quote            { if (scan_raw_string(out_error.message, this->current[-1], out_token_value, out_string_delim)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::String; } else { set_error(); return TokenType::Invalid; } }
    str_base64                      { if (scan_base64_string(out_error.message, out_token_value)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::ByteString; } else { set_error(); return TokenType::Invalid; } }
    str_datetime                    { set_token(); return TokenType::DateTime; }
    str_datetime_prefix             { if (scan_datetime_string(out_error.message, out_token_value)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::DateTime; } else { set_error(); return TokenType::Invalid; } }
    quote				            { if (scan_string(out_error.message, this->current[-1], out_token_value)) { get_token_pos(out_start_idx, out_end_idx); return TokenType::String; } else { set_error(); return TokenType::Invalid; } }

    // object keys
    "true" / whitespace* ":"        { set_token(); return TokenType::True; }
    "false" / whitespace* ":"       { set_token(); return TokenType::False; }
    "null" / whitespace* ":"        { set_token(); return TokenType::Null; }
    object_key / whitespace* ":"    { set_token(); return TokenType::Identifier; }

    // numbers
    "nan"                           { set_token(); return TokenType::Number; }
    (minus | plus)? "inf"           { set_token(); return TokenType::Number; }
    number_value                    { set_token(); return TokenType::Number; }

    // identifiers
    identifier                      { set_token(); return TokenType::Identifier; }

    spaces                          { goto regular; }
    linebreak (spaces linebreak)*   { set_token(); return TokenType::LineBreak; }

    * {
        if (this->current >= this->limit)
        {
            goto end_of_stream;
        }
        else
        {
            set_error_msg("Invalid syntax"); return TokenType::Invalid;
        }
    }

    */

end_of_stream:
    set_token();
    return TokenType::EndOfStream;
}

JXC_END_NAMESPACE(jxc)
