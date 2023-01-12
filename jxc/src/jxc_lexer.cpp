#include "jxc/jxc_lexer.h"
#include "jxc/jxc_format.h"


JXC_BEGIN_NAMESPACE(jxc)


bool Lexer::scan_comment(size_t comment_token_len, std::string_view& out_comment)
{
    const char* start = reinterpret_cast<const char*>(this->current - comment_token_len);
    JXC_DEBUG_ASSERT(start[0] == '#' || (start[0] == '/' && start[1] == '/'));
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


bool Lexer::scan_raw_string(uint8_t quote_char, std::string_view& out_string, std::string_view& out_delim)
{
    JXC_DEBUG_ASSERT(*(this->current - 2) == 'r');
    JXC_DEBUG_ASSERT(*(this->current - 1) == quote_char);

    const uint8_t* raw_str_start = this->current - 2;

    // find the delimiter string (if any)
    const uint8_t* delimiter = this->current;
    size_t delimiter_len = 0;
    while (*this->current != '(' && this->current < this->limit)
    {
        ++this->current;
        ++delimiter_len;
    }

    out_string = std::string_view{};
    out_delim = std::string_view{ reinterpret_cast<const char*>(delimiter), delimiter_len };

    // minimum number of chars remaining is 2: `)"`
    if (this->current + 2 > this->limit)
    {
        return false;
    }

    JXC_DEBUG_ASSERT((*this->current) == '(');
    ++this->current;

    // scan forward until we find `){delimiter}{quote_char}`
    size_t raw_string_len = 0;
    bool found_end = false;

    if (delimiter_len > 0)
    {
        // scan for the delimiter
        while (this->current < this->limit && !found_end)
        {
            // scan for the next close paren
            while (this->current < this->limit && *this->current != ')')
            {
                ++this->current;
            }

            if (detail::delimiter_matches(this->current + 1, this->limit, delimiter, delimiter_len))
            {
                // found the closing delimiter
                break;
            }
            else
            {
                // this close paren was just part of the string - find the next one and try again
                ++this->current;
            }
        }

        if (this->current < this->limit && *this->current == ')')
        {
            this->current += 1; // skip over close paren
            this->current += delimiter_len; // skip over delimiter
                
            // we should now be at the closing quote char
            if (this->current <= this->limit && *this->current == quote_char)
            {
                ++this->current;
                const int64_t len = (int64_t)(this->current - raw_str_start);
                raw_string_len = (len >= 0) ? (size_t)len : 0;
                found_end = true;
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        // no delimiter - just scan for the end of the string, and also don't allow newlines in this case.
        while (this->current < this->limit && *this->current != ')')
        {
            ++this->current;
        }

        while (this->current < this->limit && *this->current != quote_char)
        {
            ++this->current;
        }

        if (*this->current == quote_char)
        {
            ++this->current;
            const int64_t len = (int64_t)(this->current - raw_str_start);
            raw_string_len = (len >= 0) ? (size_t)len : 0;
            found_end = true;
        }
    }

    if (found_end)
    {
        out_string = std::string_view{ reinterpret_cast<const char*>(raw_str_start), raw_string_len };
        return true;
    }

    return false;
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


JXC_BEGIN_NAMESPACE(detail)

bool delimiter_matches(const uint8_t* start, const uint8_t* buf_end, const uint8_t* delimiter, size_t delimiter_len)
{
    if (start + delimiter_len > buf_end)
    {
        return false;
    }

    for (size_t i = 0; i < delimiter_len; ++i)
    {
        if (*(start + i) != *(delimiter + i))
        {
            return false;
        }
    }

    return true;
}

JXC_END_NAMESPACE(detail)

JXC_END_NAMESPACE(jxc)
