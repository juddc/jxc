#include "jxc/jxc_lexer.h"
#include "jxc/jxc_format.h"


static inline bool is_valid_heredoc_first_char(char ch)
{
    return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}


static inline bool is_valid_heredoc_char(char ch)
{
    return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}


JXC_BEGIN_NAMESPACE(jxc)


bool Lexer::scan_comment(size_t comment_token_len, std::string_view& out_comment)
{
    const char* start = reinterpret_cast<const char*>(this->current - comment_token_len);
    JXC_DEBUG_ASSERT(start[0] == '#');
    while (this->current < this->limit && *this->current != '\n')
    {
        ++this->current;
    }
    const int64_t comment_len = reinterpret_cast<const char*>(this->current) - start;
    if (comment_len >= 0)
    {
        out_comment = std::string_view{ start, static_cast<size_t>(comment_len) };
        return true;
    }
    return false;
}


bool Lexer::scan_hex_escape(std::string& out_error_message)
{
    JXC_DEBUG_ASSERT(*this->current == 'x');
    ++this->current;
    size_t hex_digit_count = 0;
    while (hex_digit_count < 2 && this->current < this->limit)
    {
        switch (*this->current)
        {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        default:
            out_error_message = jxc::format("Invalid hex escape character {}", detail::debug_char_repr((uint32_t)*this->current));
            return false;
        }

        ++hex_digit_count;
        ++this->current;
    }
    return true;
}


bool Lexer::scan_utf16_escape(std::string& out_error_message)
{
    JXC_DEBUG_ASSERT(*this->current == 'u');
    ++this->current;
    size_t hex_digit_count = 0;
    while (hex_digit_count < 4 && this->current < this->limit)
    {
        switch (*this->current)
        {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        default:
            out_error_message = jxc::format("Invalid utf16 escape character {}", detail::debug_char_repr((uint32_t)*this->current));
            return false;
        }

        ++hex_digit_count;
        ++this->current;
    }
    return true;
}


bool Lexer::scan_utf32_escape(std::string& out_error_message)
{
    JXC_DEBUG_ASSERT(*this->current == 'U');
    ++this->current;
    size_t hex_digit_count = 0;
    while (hex_digit_count < 8 && this->current < this->limit)
    {
        switch (*this->current)
        {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        default:
            out_error_message = jxc::format("Invalid utf32 escape character {}", detail::debug_char_repr((uint32_t)*this->current));
            return false;
        }

        ++hex_digit_count;
        ++this->current;
    }
    return true;
}


bool Lexer::scan_string(std::string& out_error_message, uint8_t quote_char, std::string_view& out_string)
{
	JXC_DEBUG_ASSERTF(*(this->current - 1) == quote_char, "prev token must be the string start char");

	// empty string case
	if (*this->current == quote_char)
	{
		out_string = std::string_view{ reinterpret_cast<const char*>(this->current - 1), 2 };
		++this->current;
		return true;
	}

    const char* start = reinterpret_cast<const char*>(this->current - 1);

    while (this->current < this->limit && *this->current != quote_char)
    {
        switch (*this->current)
        {
        case '\\':
            // check the escape char type
            ++this->current;

            // except for the addition of a single-quote escape, this is intended to be exactly the same as the JSON spec for string escapes
            switch (*this->current)
            {
            case '\"': // double-quote
            case '\'': // single-quote
            case '\\': // backslash
            case '/': // forward slash
            case 'b': // backspace
            case 'f': // formfeed
            case 'n': // line break
            case 'r': // carriage return
            case 't': // horizontal tab
                ++this->current;
                continue;

            case 'x': // hex escape (followed by 2 hex digits)
                if (!scan_hex_escape(out_error_message))
                {
                    return false;
                }
                continue;

            case 'u': // utf16 (followed by 4 hex digits)
                if (!scan_utf16_escape(out_error_message))
                {
                    return false;
                }
                continue;

            case 'U': // utf32 (followed by 8 hex digits)
                if (!scan_utf32_escape(out_error_message))
                {
                    return false;
                }
                continue;

            }
            break;

        case '\n':
            // can't have a line break in the middle of a regular string (only raw strings can do that)
            out_error_message = "Encountered line break inside non-raw string";
            return false;

        default:
            break;
        }

        ++this->current;
    }

    if (this->current >= this->limit)
    {
        out_error_message = "End of stream reached while parsing string";
        return false;
    }

    JXC_DEBUG_ASSERT(*this->current == quote_char);
    ++this->current;

    const char* end = reinterpret_cast<const char*>(this->current);
    JXC_DEBUG_ASSERT(end > start);
    const int64_t len = (end - start);
    JXC_DEBUG_ASSERT(len >= 0);
    out_string = std::string_view{ start, (size_t)len };
    return true;
}


bool Lexer::scan_raw_string(std::string& out_error_message, uint8_t quote_char, std::string_view& out_string, std::string_view& out_delim)
{
    JXC_DEBUG_ASSERT(*(this->current - 2) == 'r');
    JXC_DEBUG_ASSERT(*(this->current - 1) == quote_char);

    out_string = std::string_view{};

    const uint8_t* raw_str_start = this->current - 2;

    // max delimiter/heredoc length
    const size_t max_delimiter_length = 15;

    // scan past the end of the delimiter if we don't find a `(` char - this is only to improve error messages.
    const size_t max_delimiter_scan_length = 64;

    const uint8_t* delimiter = this->current;
    size_t delimiter_len = 0;
    if (*this->current != '(')
    {
        if (!is_valid_heredoc_first_char(static_cast<char>(*this->current)))
        {
            out_error_message = jxc::format("Failed parsing raw string: expected raw string delimiter or `(`, got {}",
                detail::debug_char_repr(static_cast<uint32_t>(*this->current), '`'));
            return false;
        }

        ++delimiter_len;
        ++this->current;

        // find the rest of the delimiter string
        bool delimiter_valid = true;
        while (this->current < this->limit && *this->current != '(' && delimiter_len < max_delimiter_scan_length)
        {
            if (!is_valid_heredoc_char(static_cast<char>(*this->current)))
            {
                // don't break here - we want to report the full invalid delimiter in the error message
                delimiter_valid = false;
            }

            ++this->current;
            ++delimiter_len;
        }

        out_delim = std::string_view{ reinterpret_cast<const char*>(delimiter), delimiter_len };

        if (!delimiter_valid)
        {
            out_error_message = jxc::format("Invalid raw string delimiter {}", detail::debug_string_repr(out_delim, '`'));
            return false;
        }
        else if (out_delim.size() > max_delimiter_length)
        {
            out_error_message = jxc::format("Raw string delimiter length is {} (max length {})", out_delim.size(), max_delimiter_length);
            return false;
        }

        JXC_DEBUG_ASSERT((*this->current) == '(');
    }
    else
    {
        out_delim = std::string_view{};
    }

    auto delimiter_matches = [this, &out_delim, &delimiter_len](const uint8_t* start)
    {
        if (start + delimiter_len > this->limit)
        {
            return false;
        }

        for (size_t i = 0; i < delimiter_len; ++i)
        {
            if (*(start + i) != out_delim[i])
            {
                return false;
            }
        }

        return true;
    };

    // minimum number of chars remaining is 2: `)"`
    if (this->current + 2 > this->limit)
    {
        out_error_message = "End of stream reached while parsing string";
        return false;
    }

    JXC_DEBUG_ASSERT((*this->current) == '(');
    ++this->current;

    // scan forward until we find `){delimiter}{quote_char}`
    size_t raw_string_len = 0;

    if (delimiter_len > 0)
    {
        // scan for the delimiter
        bool found_rhs_delimiter = false;
        while (this->current < this->limit)
        {
            // scan for the next close paren
            while (this->current < this->limit && *this->current != ')')
            {
                ++this->current;
            }

            if (delimiter_matches(this->current + 1))
            {
                // found the closing delimiter
                found_rhs_delimiter = true;
                break;
            }
            else
            {
                // this close paren was just part of the string - find the next one and try again
                ++this->current;
            }
        }

        if (!found_rhs_delimiter)
        {
            out_error_message = jxc::format("Failed parsing raw string: delimiter {} not found at end of string",
                detail::debug_string_repr(out_delim, '`'));
            return false;
        }

        // validate the end of the string
        if (this->current < this->limit && *this->current == ')')
        {
            this->current += 1; // skip over close paren
            this->current += delimiter_len; // skip over delimiter
                
            // we should now be at the closing quote char
            if (*this->current == quote_char)
            {
                ++this->current;
                const int64_t len = (int64_t)(this->current - raw_str_start);
                raw_string_len = (len >= 0) ? (size_t)len : 0;
            }
            else
            {
                out_error_message = jxc::format("Failed parsing raw string: expected end quote character `{}`, got {}",
                    (char)quote_char, detail::debug_char_repr((char)*this->current));
                return false;
            }
        }
        else
        {
            out_error_message = jxc::format("Failed parsing raw string: end of string not found (expected `){}{}`)", out_delim, (char)quote_char);
            return false;
        }
    }
    else
    {
        // no delimiter - just scan for the end of the string
        while (this->current < this->limit)
        {
            const uint8_t prev_char = *this->current;

            ++this->current;

            if (prev_char == ')' && *this->current == quote_char)
            {
                break;
            }
        }

        if (*this->current == quote_char)
        {
            ++this->current;
            const int64_t len = (int64_t)(this->current - raw_str_start);
            raw_string_len = (len >= 0) ? (size_t)len : 0;
        }
        else
        {
            out_error_message = "End of string not found";
            return false;
        }
    }

    JXC_DEBUG_ASSERT(this->current <= this->limit && *(this->current - 1) == quote_char);

    out_string = std::string_view{ reinterpret_cast<const char*>(raw_str_start), raw_string_len };
    return true;
}


bool Lexer::scan_base64_string(std::string& out_error_message, std::string_view& out_string_token)
{
    JXC_DEBUG_ASSERT(this->current - 1 >= this->start);
    const bool is_multiline = static_cast<char>(*(this->current - 1)) == '(';
    const uint8_t* str_start = nullptr;
    const uint8_t* base64_data_start = this->current;
    char quote_char = '\0';
    size_t num_base64_chars = 0;

    auto error_invalid_base64_char = [&out_error_message](char ch) -> bool
    {
        out_error_message = jxc::format("Invalid base64 string: {} is not a valid base64 character.", detail::debug_char_repr(ch));
        return false;
    };

    if (is_multiline)
    {
        JXC_DEBUG_ASSERT(this->current - 5 >= this->start);
        JXC_DEBUG_ASSERT(*(this->current - 5) == 'b');
        JXC_DEBUG_ASSERT(*(this->current - 4) == '6');
        JXC_DEBUG_ASSERT(*(this->current - 3) == '4');
        JXC_DEBUG_ASSERT(*(this->current - 2) == '\'' || *(this->current - 2) == '"');
        JXC_DEBUG_ASSERT(*(this->current - 1) == '(');

        str_start = this->current - 5;
        quote_char = static_cast<char>(*(this->current - 2));

        while (this->current < this->limit)
        {
            const char ch = static_cast<char>(*this->current);
            switch (ch)
            {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                // skip over whitespace
                break;
            case ')':
                // got a close paren followed by the correct quote char, so we're done
                if (this->current < this->limit && static_cast<char>(*(this->current + 1)) == quote_char)
                {
                    ++this->current;
                    goto parse_success;
                }
                else
                {
                    return error_invalid_base64_char(ch);
                }
            default:
                if (base64::is_base64_char(ch))
                {
                    ++num_base64_chars;
                }
                else
                {
                    return error_invalid_base64_char(ch);
                }
                break;
            }

            ++this->current;
        }
    }
    else
    {
        JXC_DEBUG_ASSERT(this->current - 4 >= this->start);
        JXC_DEBUG_ASSERT(*(this->current - 4) == 'b');
        JXC_DEBUG_ASSERT(*(this->current - 3) == '6');
        JXC_DEBUG_ASSERT(*(this->current - 2) == '4');
        JXC_DEBUG_ASSERT(*(this->current - 1) == '\'' || *(this->current - 1) == '"');

        str_start = this->current - 4;
        quote_char = static_cast<char>(*(this->current - 1));

        // no inner set of parens means no whitespace allowed
        while (this->current < this->limit)
        {
            const char ch = static_cast<char>(*this->current);
            if (ch == quote_char)
            {
                const int64_t data_len = (int64_t)(this->current - base64_data_start);
                JXC_ASSERT(data_len >= 0);
                num_base64_chars = static_cast<size_t>(data_len);
                goto parse_success;
            }
            else if (!base64::is_base64_char(ch))
            {
                return error_invalid_base64_char(ch);
            }

            ++this->current;
        }
    }

    // if we fell off the end of the parsing while loop without finding the end of the string, then this can't be valid
    out_error_message = "Unexpected end of stream while parsing base64 string";
    return false;

parse_success:
    // this->current should point at the end quote character now.
    // Advance exactly once so that current points to the character just AFTER this token.
    JXC_DEBUG_ASSERT(this->current > str_start && this->current < this->limit && static_cast<char>(*this->current) == quote_char);
    ++this->current;

    const int64_t len = (int64_t)(this->current - str_start);
    const size_t string_token_len = (len >= 0) ? (size_t)len : 0;
    out_string_token = std::string_view{ reinterpret_cast<const char*>(str_start), string_token_len };

    // valid base64 strings consist of sets of 4 characters, where each set of 4 characters is 3 bytes (or less than 3 bytes with some padding)
    if ((num_base64_chars % 4) != 0)
    {
        out_error_message = jxc::format("Invalid base64 string: length must be a multiple of 4 (got {} base64 chars)", num_base64_chars);
        return false;
    }

    return true;
}


bool Lexer::scan_datetime_string(std::string& out_error_message, std::string_view& out_datetime_token)
{
    JXC_DEBUG_ASSERT(this->current - 1 >= this->start);
    const uint8_t* str_start = nullptr;

    JXC_DEBUG_ASSERT(this->current - 3 >= this->start);
    JXC_DEBUG_ASSERT(*(this->current - 3) == 'd');
    JXC_DEBUG_ASSERT(*(this->current - 2) == 't');
    JXC_DEBUG_ASSERT(*(this->current - 1) == '\'' || *(this->current - 1) == '\"');

    str_start = this->current - 3;
    const char quote_char = static_cast<char>(*(this->current - 1));
    bool found_end_quote = false;

    while (this->current < this->limit)
    {
        const char ch = static_cast<char>(*this->current);
        if (ch == quote_char)
        {
            const int64_t data_len = (int64_t)(this->current - str_start);
            JXC_ASSERT(data_len >= 0);
            found_end_quote = true;
            break;
        }
        ++this->current;
    }

    if (!found_end_quote)
    {
        out_error_message = "Unexpected end of stream while parsing datetime string";
        return false;
    }

    // this->current should point at the end quote character now.
    // Advance exactly once so that when we return, this->current points to the character just AFTER this token.
    JXC_DEBUG_ASSERT(this->current > str_start && this->current <= this->limit && static_cast<char>(*this->current) == quote_char);
    ++this->current;

    const int64_t len = (int64_t)(this->current - str_start);
    const size_t string_token_len = (len >= 0) ? (size_t)len : 0;
    out_datetime_token = std::string_view{ reinterpret_cast<const char*>(str_start), string_token_len };

    out_error_message = "Invalid datetime string";
    return false;

#if 0

    const std::string_view value = out_datetime_token.substr(3, out_datetime_token.size() - 4);


    std::string_view year_str;
    std::string_view month_str;
    std::string_view day_str;
    std::string_view hour_str;
    std::string_view min_str;
    std::string_view sec_str; // includes fractional component
    std::string_view timezone_str; // starts with 'Z', '+', or '-'

    auto is_number = [](char ch) { return ch >= '0' && ch <= '9'; };

    auto make_error = [&out_error_message](std::string&& msg)
    {
        out_error_message = jxc::format("Invalid datetime string: {}", msg);
        return false;
    };

    auto require_char = [&](size_t index, char required_char) -> bool
    {
        if (index >= value.size())
        {
            out_error_message = "Unexpected end of stream while parsing datetime";
            return false;
        }
        else if (value[index] != required_char)
        {
            out_error_message = jxc::format("Expected character {}, got {}", detail::debug_char_repr(required_char)), detail::debug_char_repr(value[index]);
            return false;
        }
        return true;
    };

    auto read_number = [&](size_t start_index, int min_digits, int max_digits, std::string_view& out_value) -> bool
    {
        size_t i = start_index;
        while (i < value.size() && is_number(value[i]))
        {
            ++i;
        }

        const int len = static_cast<int>(i - start_index);
        if (len < min_digits || len > max_digits)
        {
            out_error_message = (min_digits == max_digits)
                ? jxc::format("Expected exactly {} digits, got {}", min_digits, len)
                : jxc::format("Expected {}-{} digits, got {}", min_digits, max_digits, len);
            return false;
        }

        out_value = value.substr(start_index, tok_len);
    };

    size_t idx = 0;
    size_t tok_start = 0;
    size_t tok_len = 0;

    while (idx < value.size() && is_number(value[idx])) { ++tok_len; ++idx; }
    year_str = value.substr(tok_start, tok_len);

    if (!require_char(idx, '-')) { return false; }

#endif

#if 0

    // valid datetime string lengths:
    // 2023-02-08 (10 chars)
    // 2023-02-08T19:10:06Z (20 chars)
    // 2023-02-08T19:10:06+00:00 (25 chars)
    if (datetime_value.size() != 10 && datetime_value.size() != 20 && datetime_value.size() != 25)
    {
        out_error_message = jxc::format("Invalid datetime string: length must be exactly 10, 20, or 25, got length {}", datetime_value.size());
        return false;
    }

    auto is_number = [](char ch)
    {
        return ch >= '0' && ch <= '9';
    };

    auto set_char_error = [&out_error_message](size_t index, char expected, char actual)
    {
        out_error_message = jxc::format("Invalid datetime string: expected character at position {} to be {}, got {}",
            index,
            detail::debug_char_repr(expected),
            detail::debug_char_repr(actual));
    };

    // validate string contents
    for (size_t i = 0; i < datetime_value.size(); i++)
    {
        const char ch = datetime_value[i];
        switch (i)
        {
        // year:
        case 0:
        case 1:
        case 2:
        case 3:
        // month:
        case 5:
        case 6:
        // day:
        case 8:
        case 9:
        // hour:
        case 11:
        case 12:
        // minute:
        case 14:
        case 15:
        // second:
        case 17:
        case 18:
        // timezone_hour:
        case 20:
        case 21:
        // timezone_minute:
        case 23:
        case 24:
            if (!is_number(ch))
            {
                out_error_message = jxc::format("Invalid datetime string: expected character at position {} to be a number, got {}", i, detail::debug_char_repr(ch));
                return false;
            }
            break;
        
        // date separators:
        case 4:
        case 7:
            if (ch != '-')
            {
                set_char_error(i, '-', ch);
                return false;
            }
            break;
        
        // date/time separator:
        case 10:
            if (ch != 'T')
            {
                set_char_error(i, 'T', ch);
                return false;
            }
            break;
        
        // time separators (in time and timezone components):
        case 13:
        case 16:
        case 22:
            if (ch != ':')
            {
                set_char_error(i, ':', ch);
                return false;
            }
            break;
        
        case 19:
            if (ch != '+' && ch != '-' && ch != 'Z')
            {
                out_error_message = jxc::format("Invalid datetime string: expected character at position {} to be {}, {}, or {}, got {}",
                    i, detail::debug_char_repr('+'), detail::debug_char_repr('-'), detail::debug_char_repr('Z'), detail::debug_char_repr(ch));
                return false;
            }
            break;
        }
    }

#endif

    return true;
}


bool AnnotationLexer::next(Token& out_token)
{
    const size_t token_idx = num_tokens;
    ++num_tokens;

    out_token.type = lex.next(lex_error, out_token.start_idx, out_token.end_idx, tok_value, tok_tag);

    if (req_next_token_type != TokenType::Invalid)
    {
        if (out_token.type != req_next_token_type)
        {
            return set_parse_error(out_token, jxc::format("Expected {}, got {}", token_type_to_string(req_next_token_type), token_type_to_string(out_token.type)));
        }
        req_next_token_type = TokenType::Invalid;
    }

    switch (out_token.type)
    {
    case TokenType::EndOfStream:
        break;
    case TokenType::ExclamationPoint:
        // allowed as first token only, unless inside angle brackets
        if (token_idx > 0 && lex.angle_bracket_depth == 0)
        {
            return set_parse_error(out_token, "`!` only allowed inside angle brackets or at the front of the annotation");
        }
        break;
    case TokenType::Identifier:
        break;
    case TokenType::Period:
        if (lex.angle_bracket_depth == 0 && last_token_type != TokenType::Identifier)
        {
            return set_parse_error(out_token, "`.` may only come after an identifier");
        }
        req_next_token_type = TokenType::Identifier;
        break;
    case TokenType::AngleBracketOpen:
        if (token_idx == 0)
        {
            return set_parse_error(out_token, "Annotations may not begin with an angle bracket");
        }
        else if (lex.angle_bracket_depth == 1 && got_complete_angle_bracket_set)
        {
            return set_parse_error(out_token, "Got second set of angle brackets");
        }
        break;
    case TokenType::AngleBracketClose:
        if (lex.angle_bracket_depth < 0)
        {
            return set_parse_error(out_token, "Unexpected close angle bracket");
        }
        else if (lex.angle_bracket_depth == 0)
        {
            got_complete_angle_bracket_set = true;
        }
        break;
    case TokenType::ParenOpen:
        if (lex.angle_bracket_depth <= 0)
        {
            return set_parse_error(out_token, "Parentheses only allowed inside angle brackets");
        }
        break;
    case TokenType::ParenClose:
        if (lex.expr_paren_depth <= 0)
        {
            return set_parse_error(out_token, "Unexpected close parentheses");
        }
        break;

    case TokenType::Asterisk:
    case TokenType::QuestionMark:
    case TokenType::Pipe:
    case TokenType::Ampersand:
    case TokenType::Equals:
    case TokenType::Comma:
    case TokenType::True:
    case TokenType::False:
    case TokenType::Null:
    case TokenType::Number:
    case TokenType::String:
    case TokenType::ByteString:
    case TokenType::DateTime:
        if (lex.angle_bracket_depth <= 0)
        {
            return set_parse_error(out_token, jxc::format("Token {} only allowed inside angle brackets", token_type_to_string(out_token.type)));
        }
        break;
    default:
        return set_parse_error(out_token, jxc::format("Unexpected token {}", token_type_to_string(out_token.type)));
    }

    out_token.value = tok_value;
    out_token.tag = tok_tag;
    last_token_type = out_token.type;
    return out_token.type != TokenType::EndOfStream && !lex_error.is_err;
}


bool ExpressionLexer::next(Token& out_token)
{
    out_token.type = lex.expr_next(lex_error, out_token.start_idx, out_token.end_idx, tok_value, tok_tag, true);
    out_token.value = tok_value;
    out_token.tag = tok_tag;
    return out_token.type != TokenType::EndOfStream && !lex_error.is_err;
}


JXC_END_NAMESPACE(jxc)
