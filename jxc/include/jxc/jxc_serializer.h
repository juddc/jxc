#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "jxc/jxc_array.h"
#include "jxc/jxc_bytes.h"
#include "jxc/jxc_util.h"


JXC_BEGIN_NAMESPACE(jxc)


class IOutputBuffer
{
public:
    virtual ~IOutputBuffer() {}
    virtual void write(const char* value, size_t value_len) = 0;
    virtual void clear() = 0;
};


class StringOutputBuffer : public IOutputBuffer
{
    std::vector<char> buf;

public:
    virtual ~StringOutputBuffer() {}

    void write(const char* value, size_t value_len) override
    {
        const size_t prev_size = buf.size();
        buf.resize(buf.size() + value_len);
        JXC_STRNCPY(buf.data() + prev_size, buf.size() - prev_size, value, value_len);
    }

    void clear() override
    {
        buf.clear();
    }

    std::string to_string() const
    {
        return std::string(buf.data(), buf.size());
    }
};


JXC_BEGIN_NAMESPACE(detail)


struct OutputBuffer
{
    IOutputBuffer* output;

    // buffered output so we're not calling a virtual function every time we write a single character
    FixedArray<char, 255> output_buffer;

    char last_char_written = '\0';

private:
    inline void flush_internal()
    {
        output->write(output_buffer.data(), output_buffer.size());
        output_buffer.clear();
    }

public:
    size_t write(std::string_view str);
    size_t write(char ch);
    size_t write(char a, char b);
    inline void flush() { if (output_buffer.size() > 0) { flush_internal(); } }

    inline char get_last_char() const { return last_char_written; }

    inline void clear() { output_buffer.clear(); output->clear(); }

    OutputBuffer(IOutputBuffer* output) : output(output) { JXC_ASSERT(output != nullptr); }
    ~OutputBuffer() { flush(); }
};


inline bool find_linebreak(const char* str, size_t max_len)
{
    if (max_len == 0)
    {
        return false;
    }
    for (size_t i = 0; i < max_len; i++)
    {
        switch (str[i])
        {
        case '\n': return true;
        case '\0': return false;
        default: break;
        }
    }
    return false;
}


JXC_END_NAMESPACE(detail)


class ExpressionProxy;


class Serializer
{
    friend class ExpressionProxy;

    SerializerSettings settings;
    int32_t indent_width = 0;
    size_t last_token_size = 0;
    bool value_separator_has_linebreak = false;

    detail::OutputBuffer output;

    enum StackType : uint8_t
    {
        ST_Invalid = 0,
        ST_Array,
        ST_Expr,
        ST_Obj,
    };

    struct StackVars
    {
        StackType type = ST_Invalid;

        // if false, waiting for object key. otherwise waiting for object value.
        bool pending_value = false;

        int64_t container_size = 0;
        
        bool suppress_next_separator = false;

        static constexpr size_t max_separator_len = 4;
        static constexpr size_t separator_buf_len = max_separator_len + 1;
        char separator[separator_buf_len] = { 0 };

        bool value_separator_has_linebreak = false;

        void set_separator(std::string_view new_sep)
        {
            JXC_STRNCPY(&separator[0], separator_buf_len, new_sep.data(), std::min(max_separator_len, new_sep.size()));
            value_separator_has_linebreak = detail::find_linebreak(separator, max_separator_len);
        }

        inline size_t separator_len() const
        {
            size_t result = 0;
            while (result < max_separator_len + 1 && separator[result] != '\0')
            {
                ++result;
            }
            return result;
        }

        std::string_view get_separator() const
        {
            return separator[0] != '\0' ? std::string_view{ &separator[0], separator_len() } : std::string_view{};
        }

        StackVars(StackType type = ST_Invalid) : type(type) {}
    };

    detail::ArrayBuffer<StackVars, 255> container_stack;

    inline StackVars& container_stack_top() { return container_stack.back(); }
    inline const StackVars& container_stack_top() const { return container_stack.back(); }

    detail::ArrayBuffer<char, 255> annotation_buffer;

    inline void set_annotation_buffer(const char* str, size_t str_len)
    {
        annotation_buffer.resize(str_len + 1);
        JXC_STRNCPY(annotation_buffer.data(), annotation_buffer.size(), str, str_len);
        annotation_buffer.back() = '\0';
    }

    inline void clear_annotation_buffer()
    {
        if (annotation_buffer.size() > 0)
        {
            annotation_buffer.front() = '\0';
        }
    }

    inline bool have_annotation_in_buffer() const
    {
        return annotation_buffer.size() > 0 && annotation_buffer.front() != '\0';
    }

    inline size_t annotation_buffer_len() const
    {
        return (annotation_buffer.size() > 0 && annotation_buffer.front() != '\0') ? strlen(annotation_buffer.data()) : 0;
    }

    inline size_t flush_annotation_buffer()
    {
        if (annotation_buffer.size() == 0 || annotation_buffer.front() == '\0')
        {
            return 0;
        }
        const size_t anno_size = annotation_buffer_len();
        output.write(std::string_view{ annotation_buffer.data(), anno_size });
        clear_annotation_buffer();
        return anno_size;
    }

    int64_t get_current_indent_width() const
    {
        return (int64_t)container_stack.size() * indent_width;
    }

    int64_t get_num_cols_remaining_on_line() const
    {
        const int32_t tgt_length = settings.get_target_line_length();
        if (tgt_length <= 0)
        {
            return std::numeric_limits<int64_t>::max();
        }
        else if (container_stack_top().type == ST_Obj)
        {
            return tgt_length - (get_current_indent_width() + last_token_size);
        }
        else
        {
            return tgt_length - get_current_indent_width();
        }
    }

    inline std::string_view get_value_separator(bool& out_has_linebreak) const
    {
        const auto& vars = container_stack_top();
        if (vars.separator[0] != '\0')
        {
            out_has_linebreak = vars.value_separator_has_linebreak;
            return vars.get_separator();
        }
        out_has_linebreak = value_separator_has_linebreak;
        return settings.value_separator;
    }

    size_t write_indent(int64_t extra_levels = 0)
    {
        size_t len = 0;
        if (settings.indent.size() > 0)
        {
            int64_t levels = (int64_t)container_stack.size();
            if (extra_levels < 0)
            {
                levels += extra_levels;
            }
            if (levels > 0)
            {
                for (int64_t i = 0; i < levels; i++)
                {
                    switch (container_stack[i].type)
                    {
                    case ST_Obj:
                    case ST_Array:
                        len += output.write(settings.indent);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (extra_levels > 0)
            {
                for (int32_t i = 0; i < extra_levels; i++)
                {
                    len += output.write(settings.indent);
                }
            }
        }
        return len;
    }

    size_t pre_write_token(TokenType type, std::string_view post_annotation_suffix = " ");
    void post_write_token();
public:
    Serializer(IOutputBuffer* output, const SerializerSettings& settings = SerializerSettings{});
    virtual ~Serializer() {}

    inline const SerializerSettings& get_settings() const { return settings; }

    inline void clear() { output.clear(); }

    inline void flush() { output.flush(); }
    inline void done() { output.flush(); }

    Serializer& annotation(std::string_view anno);

    Serializer& value_null();

    Serializer& value_bool(bool value);

    Serializer& value_int(int64_t value, std::string_view suffix = std::string_view{});
    Serializer& value_int_hex(int64_t value, std::string_view suffix = std::string_view{});
    Serializer& value_int_oct(int64_t value, std::string_view suffix = std::string_view{});
    Serializer& value_int_bin(int64_t value, std::string_view suffix = std::string_view{});

    Serializer& value_uint(uint64_t value, std::string_view suffix = std::string_view{});
    Serializer& value_uint_hex(uint64_t value, std::string_view suffix = std::string_view{});
    Serializer& value_uint_oct(uint64_t value, std::string_view suffix = std::string_view{});
    Serializer& value_uint_bin(uint64_t value, std::string_view suffix = std::string_view{});

    Serializer& value_nan();
    Serializer& value_inf(bool negative = false);

    Serializer& value_float(double value, std::string_view suffix = std::string_view{}, int32_t precision = -1, bool fixed = false);

    Serializer& value_string(std::string_view value, StringQuoteMode quote = StringQuoteMode::Auto, bool decode_unicode = true);

    Serializer& value_string_raw(std::string_view value, StringQuoteMode quote = StringQuoteMode::Auto, std::string_view tag = std::string_view{});

    Serializer& value_bytes(const uint8_t* data, size_t data_len, StringQuoteMode quote = StringQuoteMode::Auto) { return value_bytes_base64(data, data_len, quote); }
    Serializer& value_bytes_base64(const uint8_t* data, size_t data_len, StringQuoteMode quote = StringQuoteMode::Auto);

    inline Serializer& value_bytes(BytesView data, StringQuoteMode quote = StringQuoteMode::Auto) { return value_bytes(data.data(), data.size(), quote); }
    inline Serializer& value_bytes_base64(BytesView data, StringQuoteMode quote = StringQuoteMode::Auto) { return value_bytes_base64(data.data(), data.size(), quote); }

    Serializer& value_date(Date value, StringQuoteMode quote = StringQuoteMode::Auto);
    Serializer& value_datetime(DateTime value, bool auto_strip_time = false, StringQuoteMode quote = StringQuoteMode::Auto);

    Serializer& identifier(std::string_view value);

    // if the string is a valid identifier, calls identifier(), otherwise calls value_string()
    Serializer& identifier_or_string(std::string_view value, StringQuoteMode quote = StringQuoteMode::Auto, bool decode_unicode = true);

    Serializer& comment(std::string_view value);

    Serializer& write(std::string_view value);
    Serializer& write(char value);

    Serializer& array_begin(std::string_view separator = std::string_view{});
    Serializer& array_end();
    Serializer& array_empty();

    ExpressionProxy expression_begin();
    Serializer& expression_end();
    Serializer& expression_empty();

    // key-value seperator for objects
    Serializer& object_sep();

    // shorthand version. handy for chaining (eg. doc.identifier("abc").sep().value_int(123))
    inline Serializer& sep() { return object_sep(); }

    Serializer& object_begin(std::string_view separator = std::string_view{});
    Serializer& object_end();
    Serializer& object_empty();
};



class ExpressionProxy
{
    friend class Serializer;

private:
    Serializer& parent;
    int64_t num_tokens = 0;

    ExpressionProxy(Serializer& parent) : parent(parent)
    {
        JXC_DEBUG_ASSERT(parent.container_stack_top().type == Serializer::ST_Expr);
    }

    // auto-add spacing before a token if we have existing tokens
    inline void pre()
    {
        if (parent.settings.pretty_print && num_tokens > 0 && parent.output.get_last_char() != ' ')
        {
            parent.write(' ');
        }
    }

    inline void post()
    {
        ++num_tokens;
    }

#define JXC_EXPR_TOK(EXPR) do { pre(); (EXPR); post(); return *this; } while(0)

public:
    inline ExpressionProxy& value_null() { JXC_EXPR_TOK(parent.value_null()); }
    inline ExpressionProxy& value_bool(bool value) { JXC_EXPR_TOK(parent.value_bool(value)); }
    inline ExpressionProxy& value_int(int64_t value, std::string_view suffix = std::string_view{}) { JXC_EXPR_TOK(parent.value_int(value, suffix)); }
    inline ExpressionProxy& value_int_hex(int64_t value, std::string_view suffix = std::string_view{}) { JXC_EXPR_TOK(parent.value_int_hex(value, suffix)); }
    inline ExpressionProxy& value_int_oct(int64_t value, std::string_view suffix = std::string_view{}) { JXC_EXPR_TOK(parent.value_int_oct(value, suffix)); }
    inline ExpressionProxy& value_int_bin(int64_t value, std::string_view suffix = std::string_view{}) { JXC_EXPR_TOK(parent.value_int_bin(value, suffix)); }
    inline ExpressionProxy& value_nan() { JXC_EXPR_TOK(parent.value_nan()); }
    inline ExpressionProxy& value_inf(bool negative = false) { JXC_EXPR_TOK(parent.value_inf(negative)); }
    inline ExpressionProxy& value_float(double value, std::string_view suffix = std::string_view{}, int32_t precision = 8, bool fixed = false) { JXC_EXPR_TOK(parent.value_float(value, suffix, precision, fixed)); }
    inline ExpressionProxy& value_string(std::string_view value, StringQuoteMode quote = StringQuoteMode::Auto, bool decode_unicode = true) { JXC_EXPR_TOK(parent.value_string(value, quote, decode_unicode)); }
    inline ExpressionProxy& value_string_raw(std::string_view value, StringQuoteMode quote = StringQuoteMode::Auto) { JXC_EXPR_TOK(parent.value_string_raw(value, quote)); }
    inline ExpressionProxy& value_bytes(const uint8_t* data, size_t data_len, StringQuoteMode quote = StringQuoteMode::Auto) { JXC_EXPR_TOK(parent.value_bytes(data, data_len, quote)); }
    inline ExpressionProxy& value_bytes_base64(const uint8_t* data, size_t data_len, StringQuoteMode quote = StringQuoteMode::Auto) { JXC_EXPR_TOK(parent.value_bytes_base64(data, data_len, quote)); }
    inline ExpressionProxy& value_date(Date value, StringQuoteMode quote = StringQuoteMode::Auto) { JXC_EXPR_TOK(parent.value_date(value, quote)); }
    inline ExpressionProxy& value_datetime(DateTime value, bool auto_strip_time = false, StringQuoteMode quote = StringQuoteMode::Auto) { JXC_EXPR_TOK(parent.value_datetime(value, auto_strip_time, quote)); }
    inline ExpressionProxy& identifier(std::string_view value) { JXC_EXPR_TOK(parent.identifier(value)); }
    inline ExpressionProxy& identifier_or_string(std::string_view value, StringQuoteMode quote = StringQuoteMode::Auto, bool decode_unicode = true) { JXC_EXPR_TOK(parent.identifier_or_string(value, quote, decode_unicode)); }
    inline ExpressionProxy& op(std::string_view op_value) { JXC_EXPR_TOK(parent.write(op_value)); }

#undef JXC_EXPR_TOK

    // direct write (string)
    inline ExpressionProxy& write(std::string_view value) { parent.write(value); return *this; }

    // direct write (char)
    inline ExpressionProxy& write(char value) { parent.write(value); return *this; }

    // direct write (token)
    inline ExpressionProxy& token(TokenType token_type)
    {
        // true, false, and null are multi-character tokens and we should handle whitespace correctly for them.
        // Just use their normal value functions.
        switch (token_type)
        {
        case TokenType::Null: return value_null();
        case TokenType::True: return value_bool(true);
        case TokenType::False: return value_bool(false);
        default: break;
        }

        // the rest are single-character tokens, so we can direct-write those
#if JXC_DEBUG
        std::string_view sym = std::string_view{ token_type_to_symbol(token_type) };
        JXC_DEBUG_ASSERT(sym.size() <= 1);
        parent.write(sym);
#else
        parent.write(token_type_to_symbol(token_type));
#endif
        return *this;
    }

    // helpers
    inline ExpressionProxy& comma() { parent.write(','); return *this; }
    inline ExpressionProxy& paren_open() { parent.write('('); return *this; }
    inline ExpressionProxy& paren_close() { parent.write(')'); return *this; }

    // end expression and return to the parent serializer
    inline Serializer& expression_end() { return parent.expression_end(); }
};


JXC_END_NAMESPACE(jxc)
