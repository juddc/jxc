#include "jxc/jxc_serializer.h"


inline char get_quote_char(jxc::StringQuoteMode default_mode, jxc::StringQuoteMode mode)
{
    if (mode == jxc::StringQuoteMode::Auto)
    {
        mode = default_mode;
    }
    return (mode == jxc::StringQuoteMode::Single) ? '\'' : '\"';
}


JXC_BEGIN_NAMESPACE(jxc)

JXC_BEGIN_NAMESPACE(detail)


size_t OutputBuffer::write(std::string_view str)
{
    if (str.size() == 0)
    {
        return 0;
    }

    const size_t new_output_buffer_size = output_buffer.size() + str.size();
    if (new_output_buffer_size <= output_buffer.max_size())
    {
        // string fits in the existing buffer
        const size_t orig_size = output_buffer.size();
        output_buffer.resize(new_output_buffer_size);
        memcpy(output_buffer.data() + orig_size, str.data(), str.size());
    }
    else if (str.size() < output_buffer.max_size())
    {
        // string fits in the buffer, but the buffer would overflow if we appended it
        JXC_DEBUG_ASSERT(output_buffer.size() > 0);
        flush_internal();
        output_buffer.resize(str.size());
        memcpy(output_buffer.data(), str.data(), str.size());
    }
    else
    {
        // string doesn't fit in the buffer - just flush and then write out the whole thing at once
        flush_internal();
        output->write(str.data(), str.size());
    }

    last_char_written = str.back();
    return str.size();
}


size_t OutputBuffer::write(char ch)
{
    JXC_DEBUG_ASSERT(ch != '\0');
    if (!output_buffer.push(ch))
    {
        flush_internal();
        output_buffer.push(ch);
    }
    last_char_written = ch;
    return 1;
}


size_t OutputBuffer::write(char a, char b)
{
    JXC_DEBUG_ASSERT(a != '\0' && b != '\0');

    // char tmp[] = { a, b };
    // write(std::string_view{ tmp, 2 });

    if (output_buffer.size() + 2 > output_buffer.max_size())
    {
        flush_internal();
    }

    JXC_UNUSED_IN_RELEASE const bool success_a = output_buffer.push(a);
    JXC_UNUSED_IN_RELEASE const bool success_b = output_buffer.push(b);
    JXC_DEBUG_ASSERTF(success_a && success_b, "Failed to write two-char sequence to output buffer");

    last_char_written = b;
    return 2;
}


JXC_END_NAMESPACE(detail)


Serializer::Serializer(IOutputBuffer* output, const SerializerSettings& settings)
    : settings(settings)
    , value_separator_has_linebreak(detail::find_linebreak(settings.value_separator.c_str(), settings.value_separator.size()))
    , output(output)
{
    container_stack.push(StackVars{ ST_Invalid });

    indent_width = 0;
    for (size_t i = 0; i < settings.indent.size(); i++)
    {
        if (settings.indent[i] == '\t')
        {
            indent_width += 4;
        }
        else
        {
            indent_width += 1;
        }
    }
}


size_t Serializer::pre_write_token(TokenType type, std::string_view post_annotation_suffix)
{
    auto& vars = container_stack_top();

    // if (vars.type == ST_Obj && !vars.pending_value)
    // {
    //     bool is_valid_for_key = false;
    //     switch (type)
    //     {
    //     case TokenType::Identifier:
    //     case TokenType::True:
    //     case TokenType::False:
    //     case TokenType::Null:
    //     case TokenType::Number:
    //     case TokenType::String:
    //         is_valid_for_key = true;
    //         break;
    //     default:
    //         break;
    //     }
    //     JXC_ASSERTF(is_valid_for_key, "Can't use {} as object key", token_type_to_string(type));        
    // }

    // writing a key into a non-empty object, or a value into a non-empty array, so add a separator first
    if (vars.type == ST_Array || (vars.type == ST_Obj && !vars.pending_value))
    {
        if (vars.suppress_next_separator)
        {
            vars.suppress_next_separator = false;
        }
        else
        {
            bool sep_has_linebreak = false;
            std::string_view sep = get_value_separator(sep_has_linebreak);
            if (vars.container_size > 0)
            {
                output.write(sep);
            }

            if (settings.pretty_print && sep_has_linebreak)
            {
                if (vars.container_size == 0)
                {
                    output.write(settings.linebreak);
                }
                write_indent();
            }
        }
    }

    if (vars.type == ST_Array || (vars.type == ST_Obj && !vars.pending_value))
    {
        // starting a new value inside a container?
        switch (type)
        {
        case TokenType::Identifier:
        case TokenType::True:
        case TokenType::False:
        case TokenType::Null:
        case TokenType::Number:
        case TokenType::String:
        case TokenType::ByteString:
        case TokenType::DateTime:
        case TokenType::BraceOpen:
        case TokenType::SquareBracketOpen:
        case TokenType::ParenOpen:
            ++vars.container_size;
            break;
        default:
            break;
        }
    }

    // insert annotation if one was specified
    if (have_annotation_in_buffer())
    {
        size_t prefix_len = flush_annotation_buffer();
        if (prefix_len > 0)
        {
            prefix_len += output.write(post_annotation_suffix);
        }
        return prefix_len;
    }

    return 0;
}


void Serializer::post_write_token()
{
    auto& vars = container_stack_top();
    vars.pending_value = !vars.pending_value;

    //TODO: determine this is required in some cases - in theory it shouldn't be.
    output.flush();
}


Serializer& Serializer::annotation(std::string_view anno)
{
    if (anno.size() > 0)
    {
        set_annotation_buffer(anno.data(), anno.size());
    }
    else
    {
        clear_annotation_buffer();
    }
    return *this;
}


Serializer& Serializer::value_null()
{
    last_token_size = pre_write_token(TokenType::Null);
    last_token_size += output.write("null");
    post_write_token();
    return *this;
}


Serializer& Serializer::value_bool(bool value)
{
    last_token_size = pre_write_token(value ? TokenType::True : TokenType::False);
    last_token_size += output.write(value ? "true" : "false");
    post_write_token();
    return *this;
}


Serializer& Serializer::value_int(int64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    last_token_size += output.write(jxc::format("{}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_int_hex(int64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    if (value < 0)
    {
        last_token_size += output.write('-');
        value = -value;
    }
    last_token_size += output.write(jxc::format("0x{:x}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_int_oct(int64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    if (value < 0)
    {
        last_token_size += output.write('-');
        value = -value;
    }
    last_token_size += output.write(jxc::format("0o{:o}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_int_bin(int64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    if (value < 0)
    {
        last_token_size += output.write('-');
        value = -value;
    }
    last_token_size += output.write(jxc::format("0b{:b}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_uint(uint64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    last_token_size += output.write(jxc::format("{}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_uint_hex(uint64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    last_token_size += output.write(jxc::format("0x{:x}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_uint_oct(uint64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    last_token_size += output.write(jxc::format("0o{:o}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_uint_bin(uint64_t value, std::string_view suffix)
{
    last_token_size = pre_write_token(TokenType::Number);
    last_token_size += output.write(jxc::format("0b{:b}{}", value, suffix));
    post_write_token();
    return *this;
}


Serializer& Serializer::value_nan()
{
    last_token_size = pre_write_token(TokenType::Number);
    last_token_size += output.write("nan");
    post_write_token();
    return *this;
}


Serializer& Serializer::value_inf(bool negative)
{
    last_token_size = pre_write_token(TokenType::Number);
    if (negative)
    {
        last_token_size += output.write("-inf");
    }
    else
    {
        last_token_size += output.write("inf");
    }
    post_write_token();
    return *this;
}


Serializer& Serializer::value_float(double value, std::string_view suffix, int32_t precision, bool fixed)
{
    // handle float literals
    switch (get_float_literal_type(value))
    {
    case FloatLiteralType::NotANumber:
        return value_nan();
    case FloatLiteralType::PosInfinity:
        return value_inf();
    case FloatLiteralType::NegInfinity:
        return value_inf(true);
    default:
        break;
    }

    JXC_ASSERTF(suffix.size() <= 15, "Numeric suffix length must be <= 15 (got suffix of length {})", suffix.size());

    last_token_size = pre_write_token(TokenType::Number);

    if (precision < 0)
    {
        precision = settings.default_float_precision;
    }

    if (precision < 0)
    {
        precision = 0;
    }

    std::string buf;
    if (precision == 0)
    {
        // precision is zero, just round to int
        buf = jxc::format("{}", static_cast<int64_t>(std::round(value)));
    }
    else
    {
        buf = jxc::format("{}", FloatPrecision(value, precision));

        // remove trailing zeros as long as there are multiple zeroes in a row
        std::string_view buf_view = buf;
        if (!fixed && !settings.float_fixed_precision)
        {
            while (buf_view.size() >= 2 && buf_view[buf_view.size() - 2] != '.' && buf_view[buf_view.size() - 1] == '0')
            {
                buf_view = buf_view.substr(0, buf_view.size() - 1);
            }
        }
        buf = std::string(buf_view);
    }

    last_token_size += output.write(buf);
    last_token_size += output.write(suffix);
    post_write_token();
    return *this;
}


Serializer& Serializer::value_string(std::string_view value, StringQuoteMode quote, bool decode_unicode)
{
    last_token_size = pre_write_token(TokenType::String);
    const char quote_char = get_quote_char(settings.default_quote, quote);
    last_token_size += output.write(quote_char);

    detail::MiniBuffer<char, 12> escaped_char_buf;

    static constexpr bool escape_backslash = true;
    const bool escape_single_quotes = quote_char == '\'';
    const bool escape_double_quotes = quote_char == '\"';

    if (decode_unicode)
    {
        const char* buf = value.data();
        const size_t buf_len = value.size();
        size_t buf_idx = 0;
        while (buf_idx < buf_len)
        {
            const uint32_t codepoint = detail::utf8::decode(buf, buf_len, buf_idx);
            if (codepoint < 0x80)
            {
                const size_t num_chars = detail::serialize_ascii_codepoint(static_cast<uint8_t>(codepoint), escaped_char_buf.data(),
                    escaped_char_buf.capacity(), escape_backslash, escape_single_quotes, escape_double_quotes);
                output.write(std::string_view{ escaped_char_buf.data(), num_chars });
                last_token_size += num_chars;
            }
            else
            {
                const size_t num_chars = detail::serialize_utf32_codepoint(codepoint, escaped_char_buf.data(), escaped_char_buf.capacity());
                output.write(std::string_view{ escaped_char_buf.data(), num_chars });
                last_token_size += num_chars;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < value.size(); i++)
        {
            const char ch = value[i];
            if (detail::is_ascii_escape_char(ch, quote_char))
            {
                const size_t num_chars = detail::serialize_ascii_codepoint(static_cast<uint8_t>(ch), escaped_char_buf.data(), escaped_char_buf.capacity(),
                    escape_backslash, escape_single_quotes, escape_double_quotes);
                last_token_size += output.write(std::string_view{ escaped_char_buf.data(), num_chars });
            }
            else
            {
                last_token_size += output.write(ch);
            }
        }
    }

    last_token_size += output.write(quote_char);
    post_write_token();
    return *this;
}


Serializer& Serializer::value_string_raw(std::string_view value, StringQuoteMode quote, std::string_view tag)
{
    const char quote_char = get_quote_char(settings.default_quote, quote);
    last_token_size = pre_write_token(TokenType::String);
    last_token_size += output.write('r', quote_char);
    last_token_size += output.write(tag);
    last_token_size += output.write('(');
    last_token_size += output.write(value);
    last_token_size += output.write(')');
    last_token_size += output.write(tag);
    last_token_size += output.write(quote_char);
    post_write_token();
    return *this;
}


Serializer& Serializer::value_bytes_base64(const uint8_t* data, size_t data_len, StringQuoteMode quote)
{
    const char quote_char = get_quote_char(settings.default_quote, quote);
    last_token_size = pre_write_token(TokenType::ByteString);
    last_token_size += output.write("b64");
    last_token_size += output.write(quote_char);

    detail::ArrayBuffer<char, 255> buf;
    if (data != nullptr && data_len > 0)
    {
        buf.resize(base64::get_base64_string_size(data_len));
        base64::bytes_to_base64(data, data_len, buf.data(), buf.size());
    }

    if (settings.pretty_print && (int64_t)buf.size() > get_num_cols_remaining_on_line())
    {
        // multi-line
        last_token_size += output.write('(');

        int64_t num_chars_per_line = settings.target_line_length - get_current_indent_width();
        if (num_chars_per_line < 20)
        {
            num_chars_per_line = 20;
        }

        size_t chars_remaining = buf.size();
        size_t offset = 0;
        while (chars_remaining > buf.size())
        {
            last_token_size += output.write(settings.linebreak);
            last_token_size += write_indent(1);
            const size_t line_num_chars = (chars_remaining > (size_t)num_chars_per_line) ? (size_t)num_chars_per_line : chars_remaining;
            last_token_size += output.write(std::string_view{ buf.data() + offset, line_num_chars });
            chars_remaining -= line_num_chars;
            offset += line_num_chars;
        }

        last_token_size += output.write(settings.linebreak);
        last_token_size += write_indent();
        last_token_size += output.write(')');
    }
    else
    {
        // single-line
        last_token_size += output.write(std::string_view{ buf.data(), buf.size() });
    }

    last_token_size += output.write(quote_char);
    post_write_token();
    return *this;
}


Serializer& Serializer::value_date(Date value, StringQuoteMode quote)
{
    const char quote_char = get_quote_char(settings.default_quote, quote);
    last_token_size = pre_write_token(TokenType::DateTime);
    last_token_size += output.write("dt");
    last_token_size += output.write(quote_char);
    last_token_size += output.write(date_to_iso8601(value));
    last_token_size += output.write(quote_char);
    post_write_token();
    return *this;
}


Serializer& Serializer::value_datetime(DateTime value, bool auto_strip_time, StringQuoteMode quote)
{
    const char quote_char = get_quote_char(settings.default_quote, quote);
    last_token_size = pre_write_token(TokenType::DateTime);
    last_token_size += output.write("dt");
    last_token_size += output.write(quote_char);
    last_token_size += output.write(datetime_to_iso8601(value, auto_strip_time));
    last_token_size += output.write(quote_char);
    post_write_token();
    return *this;
}


Serializer& Serializer::identifier(std::string_view value)
{
    last_token_size = pre_write_token(TokenType::Identifier);
    last_token_size += output.write(value);
    post_write_token();
    return *this;
}


Serializer& Serializer::identifier_or_string(std::string_view value, StringQuoteMode quote, bool decode_unicode)
{
    auto& vars = container_stack_top();
    // if we're waiting for an object key specifically, allow any valid object key, not just a plain identifier
    if (vars.type == ST_Obj && !vars.pending_value)
    {
        return is_valid_object_key(value) ? identifier(value) : value_string(value, quote, decode_unicode);
    }
    else
    {
        return is_valid_identifier(value) ? identifier(value) : value_string(value, quote, decode_unicode);
    }
}


Serializer& Serializer::comment(std::string_view value)
{
    auto& vars = container_stack_top();
    if (vars.type == ST_Array || (vars.type == ST_Obj && !vars.pending_value))
    {
        bool sep_has_linebreak = false;
        if (vars.container_size > 0)
        {
            output.write(get_value_separator(sep_has_linebreak));
        }
        vars.suppress_next_separator = true;
    }

    if (output.get_last_char() != '\n')
    {
        output.write('\n');
    }
    write_indent();
    output.write('#', ' ');
    output.write(value);
    output.write('\n');
    write_indent();
    return *this;
}


Serializer& Serializer::write(std::string_view value)
{
    output.write(value);
    return *this;
}


Serializer& Serializer::write(char value)
{
    output.write(value);
    return *this;
}


Serializer& Serializer::array_begin(std::string_view separator)
{
    last_token_size = pre_write_token(TokenType::SquareBracketOpen, "");
    last_token_size += output.write('[');
    post_write_token();
    auto& vars = container_stack.push(ST_Array);
    if (separator.size() > 0)
    {
        vars.set_separator(separator);
    }
    return *this;
}


Serializer& Serializer::array_end()
{
    auto& vars = container_stack_top();
    JXC_ASSERT(vars.type == ST_Array);
    if (vars.container_size > 0)
    {
        bool sep_has_linebreak = false;
        std::string_view sep = get_value_separator(sep_has_linebreak);
        if (sep_has_linebreak)
        {
            output.write(sep);
            write_indent(-1);
        }
    }

    container_stack.pop_back();
    output.write(']');
    return *this;
}


Serializer& Serializer::array_empty()
{
    last_token_size = pre_write_token(TokenType::SquareBracketClose, "");
    last_token_size += output.write('[', ']');
    post_write_token();
    return *this;
}


ExpressionProxy Serializer::expression_begin()
{
    last_token_size = pre_write_token(TokenType::ParenOpen, "");
    last_token_size += output.write('(');
    post_write_token();
    container_stack.push(ST_Expr);
    return ExpressionProxy(*this);
}


Serializer& Serializer::expression_end()
{
    JXC_ASSERT(container_stack_top().type == ST_Expr);
    container_stack.pop_back();
    output.write(")");
    return *this;
}


Serializer& Serializer::expression_empty()
{
    last_token_size = pre_write_token(TokenType::ParenOpen, "");
    last_token_size += output.write('(', ')');
    post_write_token();
    return *this;
}


Serializer& Serializer::object_sep()
{
    auto& vars = container_stack_top();
    //JXC_ASSERT(!vars.pending_value);
    output.write(settings.key_separator);
    if (settings.pretty_print && !detail::string_view_ends_with(settings.key_separator, ' '))
    {
        output.write(' ');
    }
    vars.pending_value = true;
    return *this;
}


Serializer& Serializer::object_begin(std::string_view separator)
{
    last_token_size = pre_write_token(TokenType::BraceOpen, "");
    last_token_size += output.write('{');
    post_write_token();
    auto& vars = container_stack.push(ST_Obj);
    if (separator.size() > 0)
    {
        vars.set_separator(separator);
    }
    return *this;
}


Serializer& Serializer::object_end()
{
    auto& vars = container_stack_top();
    JXC_ASSERT(vars.type == ST_Obj);
    if (vars.container_size > 0)
    {
        bool sep_has_linebreak = false;
        std::string_view sep = get_value_separator(sep_has_linebreak);
        if (sep_has_linebreak)
        {
            output.write(sep);
            write_indent(-1);
        }
    }
    container_stack.pop_back();
    output.write("}");
    return *this;
}


Serializer& Serializer::object_empty()
{
    last_token_size = pre_write_token(TokenType::BraceOpen, "");
    last_token_size += output.write('{', '}');
    post_write_token();
    return *this;
}

JXC_END_NAMESPACE(jxc)
