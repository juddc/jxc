#include "jxc/jxc_util.h"
#include "jxc/jxc_lexer.h"
#include <sstream>
#include <fstream>
#include <filesystem>


namespace jxc
{


std::string_view detail::get_base_filename(const char* file)
{
    std::string_view result = file;

#if _WIN32
    size_t last_dir_sep = result.find_last_of('\\');
    if (last_dir_sep == std::string_view::npos)
    {
        last_dir_sep = result.find_last_of('/');
    }
#else
    const size_t last_dir_sep = result.find_last_of('/');
#endif

    return (last_dir_sep != std::string_view::npos) ? result.substr(last_dir_sep + 1) : result;
}


std::optional<std::string> detail::read_file_to_string(const std::string& file_path, std::string* out_error)
{
    namespace fs = std::filesystem;

    if (fs::exists(file_path))
    {
        std::ifstream fp(file_path, std::ios::in);
        std::ostringstream ss;
        ss << fp.rdbuf();
        if (out_error != nullptr)
        {
            out_error->clear();
        }
        return ss.str();
    }

    if (out_error != nullptr)
    {
        *out_error = jxc::format("File does not exist {}", debug_string_repr(file_path));
    }
    return std::nullopt;
}


std::string detail::debug_string_repr(std::string_view value, char quote_char)
{
    std::ostringstream stream;

    if (quote_char != '\0')
    {
        stream << quote_char;
    }

    const size_t sz = value.size();
    if (sz > 0)
    {
        for (size_t i = 0; i < sz; i++)
        {
            stream << debug_char_repr(static_cast<uint8_t>(value[i]), '\0');
        }
    }

    if (quote_char != '\0')
    {
        stream << quote_char;
    }

    return stream.str();
}


std::string detail::debug_bytes_repr(BytesView bytes, char quote_char)
{
    std::ostringstream stream;

    if (bytes.data() == nullptr)
    {
        stream << "NULL";
    }
    else
    {
        if (quote_char != '\0')
        {
            stream << quote_char;
        }

        const size_t sz = bytes.size();
        if (sz > 0)
        {
            for (size_t i = 0; i < sz; i++)
            {
                const uint8_t ch = bytes[i];
                if (is_renderable_ascii_char(ch))
                {
                    stream << ch;
                }
                else
                {
                    char a = 0;
                    char b = 0;
                    detail::byte_to_hex(ch, a, b);
                    stream << "\\x" << a << b;
                }
            }
        }

        if (quote_char != '\0')
        {
            stream << quote_char;
        }
    }

    return stream.str();
}


std::string detail::encode_bytes_to_string(BytesView bytes)
{
    if (bytes.data() == nullptr || bytes.size() == 0)
    {
        return std::string{};
    }

    // ascii encoding buffer
    detail::MiniBuffer<char, 4> buf;

    std::ostringstream stream;
    const size_t sz = bytes.size();
    for (size_t i = 0; i < sz; i++)
    {
        const size_t codepoint_len = serialize_ascii_codepoint(static_cast<uint8_t>(bytes[i]), buf.data(), buf.capacity());
        for (size_t j = 0; j < codepoint_len; j++)
        {
            stream << buf[j];
        }
    }
    return stream.str();
}


std::string detail::debug_char_repr(uint32_t codepoint, char quote_char)
{
    std::string result;
    if (codepoint < 0x80)
    {
        result.resize(4, '\0');
        const size_t new_len = serialize_ascii_codepoint(static_cast<uint8_t>(codepoint), result.data(), result.size(),
            true, // escape backslash
            quote_char == '\'', // escape single quotes
            quote_char == '\"'); // escape double quotes
        result.resize(new_len);
    }
    else
    {
        result.resize(10, '\0');
        const size_t new_len = serialize_utf32_codepoint(codepoint, result.data(), result.size());
        result.resize(new_len);
    }

    if (quote_char != '\0')
    {
        result.insert(result.begin(), quote_char);
        result.push_back(quote_char);
    }

    return result;
}


bool detail::is_ascii_escape_char(uint32_t codepoint, char quote_char)
{
    // check for characters we need to escape - not printable, but renderable in a string
    switch (codepoint)
    {
    case '\a':
    case '\b':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
    case '\\':
        return true;
    default:
        break;
    }

    // escape the quote char for the string we're likely inside
    if (is_renderable_ascii_char(quote_char) && codepoint == static_cast<uint8_t>(quote_char))
    {
        return true;
    }

    return false;
}


size_t detail::serialize_ascii_codepoint(uint8_t codepoint, char* out_buf, size_t out_buf_len, bool escape_backslash, bool escape_single_quote, bool escape_double_quote)
{
    // we require a buffer of at least 4 chars
    JXC_DEBUG_ASSERT(out_buf_len >= 4);

    // check for characters we need to escape
    switch (codepoint)
    {
    case '\0': out_buf[0] = '\\'; out_buf[1] = '0'; return 2;
    case '\a': out_buf[0] = '\\'; out_buf[1] = 'a'; return 2;
    case '\b': out_buf[0] = '\\'; out_buf[1] = 'b'; return 2;
    case '\f': out_buf[0] = '\\'; out_buf[1] = 'f'; return 2;
    case '\n': out_buf[0] = '\\'; out_buf[1] = 'n'; return 2;
    case '\r': out_buf[0] = '\\'; out_buf[1] = 'r'; return 2;
    case '\t': out_buf[0] = '\\'; out_buf[1] = 't'; return 2;
    case '\v': out_buf[0] = '\\'; out_buf[1] = 'v'; return 2;
    case '\\':
        if (escape_backslash)
        {
            out_buf[0] = '\\';
            out_buf[1] = '\\';
            return 2;
        }
        break;
    case '\"':
        if (escape_double_quote)
        {
            out_buf[0] = '\\';
            out_buf[1] = '\"';
            return 2;
        }
        break;
    case '\'':
        if (escape_single_quote)
        {
            out_buf[0] = '\\';
            out_buf[1] = '\'';
            return 2;
        }
    default:
        break;
    }

    // is the character in the printable ascii character range?
    if (codepoint >= 32 && codepoint <= 126)
    {
        out_buf[0] = codepoint;
        return 1;
    }

    // not a printable character, escape it using a general-purpose hex escape
    out_buf[0] = '\\';
    out_buf[1] = 'x';
    out_buf[2] = detail::byte_to_hex_table[(codepoint >> 4) & 0xF];
    out_buf[3] = detail::byte_to_hex_table[codepoint & 0xF];
    return 4;
}


size_t detail::serialize_utf32_codepoint(uint32_t codepoint, char* out_buf, size_t out_buf_len)
{
    // we require a buffer of at least 10 chars
    JXC_DEBUG_ASSERT(out_buf_len >= 10);

    if (codepoint <= 0xFFFF)
    {
        out_buf[0] = '\\';
        out_buf[1] = 'u';
        out_buf[2] = detail::byte_to_hex_table[(codepoint >> 12) & 0xF];
        out_buf[3] = detail::byte_to_hex_table[(codepoint >> 8) & 0xF];
        out_buf[4] = detail::byte_to_hex_table[(codepoint >> 4) & 0xF];
        out_buf[5] = detail::byte_to_hex_table[codepoint & 0xF];
        return 6;
    }
    else
    {
        out_buf[0] = '\\';
        out_buf[1] = 'U';
        out_buf[2] = detail::byte_to_hex_table[(codepoint >> 28) & 0xF];
        out_buf[3] = detail::byte_to_hex_table[(codepoint >> 24) & 0xF];
        out_buf[4] = detail::byte_to_hex_table[(codepoint >> 20) & 0xF];
        out_buf[5] = detail::byte_to_hex_table[(codepoint >> 16) & 0xF];
        out_buf[6] = detail::byte_to_hex_table[(codepoint >> 12) & 0xF];
        out_buf[7] = detail::byte_to_hex_table[(codepoint >> 8) & 0xF];
        out_buf[8] = detail::byte_to_hex_table[(codepoint >> 4) & 0xF];
        out_buf[9] = detail::byte_to_hex_table[codepoint & 0xF];
        return 10;
    }
}


uint32_t detail::deserialize_hex_to_codepoint(const char* hex_buf, size_t hex_buf_len, std::string* out_error)
{
    uint32_t result = 0;
    if (hex_buf_len > 0)
    {
        for (int i = 0; i < (int)hex_buf_len; i++)
        {
            const uint8_t ch = static_cast<uint8_t>(hex_buf[i]);
            if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))
            {
                const int byte_offset = ((int)hex_buf_len - 1 - i) * 4;
                result |= detail::hex_char_to_byte_table[ch] << byte_offset;
            }
            else
            {
                if (out_error != nullptr)
                {
                    *out_error = jxc::format("Invalid hex character {}", debug_char_repr(ch));
                }
                return 0;
            }
        }
    }
    return result;
}


namespace detail::utf8
{


int32_t num_codepoint_bytes(uint32_t codepoint)
{
    if (codepoint < max_2byte_codepoint)
    {
        return (codepoint < max_1byte_codepoint) ? 1 : 2;
    }
    else
    {
        return (codepoint < max_3byte_codepoint) ? 3 : 4;
    }
}


uint32_t decode(const char* buf, size_t buf_len, size_t& inout_index)
{
    // Count # of leading 1 bits.
    int32_t k = buf[inout_index] ? JXC_COUNT_LEADING_ZEROS_U32(~(buf[inout_index] << 24)) : 0;
    // All 1's with k leading 0's.
    int32_t mask = (1 << (8 - k)) - 1;
    int32_t value = buf[inout_index] & mask;
    // Note that k = #total bytes, or 0.
    ++inout_index;
    --k;
    while (k > 0)
    {
        if (inout_index >= buf_len)
        {
            return error_char;
        }

        value <<= 6;
        value += (buf[inout_index] & 0x3F);

        ++inout_index;
        --k;
    }
    JXC_DEBUG_ASSERT(value >= 0);
    return static_cast<uint32_t>(value);
}


size_t encode(char* buf, size_t buf_len, size_t& inout_index, uint32_t codepoint)
{
    auto push_byte = [&buf, &inout_index, buf_len](uint8_t byte_value)
    {
        JXC_DEBUG_ASSERTF(inout_index + 1 < buf_len, "Ran out of space in output buffer while encoding utf-8 string");
        buf[inout_index] = static_cast<char>(byte_value);
        ++inout_index;
    };

    auto ensure_size = [&inout_index, buf_len](size_t num_bytes)
    {
        JXC_ASSERTF(inout_index + num_bytes < buf_len, "Ran out of space in output buffer while encoding utf-8 string");
    };

    auto encode_replacement_char = [&]() -> size_t
    {
        ensure_size(3);
        push_byte(0xef);
        push_byte(0xbf);
        push_byte(0xbd);
        return 3;
    };

    if (codepoint < 0x7f)
    {
        ensure_size(1);
        push_byte(static_cast<uint8_t>(codepoint));
        return 1;
    }
    else if (codepoint < 0x7ff)
    {
        ensure_size(2);
        push_byte(0xc0 | (codepoint >> 6));
        push_byte(0x80 | (codepoint & 0x3f));
        return 2;
    }
    else if (codepoint < 0xffff)
    {
        if (codepoint >= 0xd800 && codepoint <= 0xdfff)
        {
            return encode_replacement_char();
        }
        else
        {
            ensure_size(3);
            push_byte(0xE0 | (codepoint >> 12));
            push_byte(0x80 | ((codepoint >> 6) & 0x3f));
            push_byte(0x80 | (codepoint & 0x3f));
            return 3;
        }
    }
    else if (codepoint < 0x10ffff)
    {
        ensure_size(4);
        push_byte(0xf0 | (codepoint >> 18));
        push_byte(0x80 | ((codepoint >> 12) & 0x3f));
        push_byte(0x80 | ((codepoint >> 6) & 0x3f));
        push_byte(0x80 | (codepoint & 0x3f));
        return 4;
    }
    else
    {
        return encode_replacement_char();
    }
}


bool split_into_surrogates(uint32_t codepoint, uint32_t& out_surr1, uint32_t& out_surr2)
{
    if (codepoint <= 0xFFFF)
    {
        return false;
    }

    out_surr2 = 0xDC00 | (codepoint & 0x3FF);        // Save the low 10 bits.
    codepoint >>= 10;                                // Drop the low 10 bits.
    out_surr1 = 0xD800 | (codepoint & 0x03F);        // Save the next 6 bits.
    out_surr1 |= ((codepoint & 0x7C0) - 0x40) << 6;  // Insert the last 5 bits less 1.
    return true;
}


bool join_from_surrogates(uint32_t& inout_old, uint32_t& inout_codepoint)
{
    if (inout_old)
    {
        inout_codepoint = (((inout_old & 0x3FF) + 0x40) << 10) + (inout_codepoint & 0x3FF);
    }

    inout_old = (inout_codepoint & 0xD800) == 0xD800 ? inout_codepoint : 0;
    return inout_old != 0;
}


} // namespace detail::utf8


bool is_valid_identifier(std::string_view value)
{
    if (value.size() == 0 || !is_valid_identifier_first_char(value[0]))
    {
        return false;
    }

    for (size_t i = 1; i < value.size(); i++)
    {
        if (!is_valid_identifier_char(value[i]))
        {
            return false;
        }
    }

    return true;
}


bool is_valid_object_key(std::string_view key)
{
    // ([a-zA-Z_$*][a-zA-Z0-9_$*]*)([\.\-][a-zA-Z_$*][a-zA-Z0-9_$*]*)*')
    auto is_valid_first_char = [](char ch) { return is_valid_identifier_first_char(ch) || ch == '*'; };
    auto is_valid_char = [](char ch) { return is_valid_identifier_char(ch) || ch == '*'; };
    auto is_separator = [](char ch) { return ch == '.' || ch == '-'; };

    if (key.size() == 0 || (!is_valid_first_char(key[0]) && !is_separator(key[0])))
    {
        return false;
    }

    size_t idx = 0;
    size_t segment_len = 0;
    while (idx < key.size())
    {
        const char ch = key[idx];
        if (is_separator(ch))
        {
            segment_len = 0;
        }
        else
        {
            segment_len += 1;
            if ((segment_len == 1 && !is_valid_first_char(ch)) || (segment_len > 1 && !is_valid_char(ch)))
            {
                return false;
            }
        }

        ++idx;
    }

    return true;
}


const char* float_literal_type_to_string(FloatLiteralType type)
{
    switch (type)
    {
    case JXC_ENUMSTR(FloatLiteralType, Finite);
    case JXC_ENUMSTR(FloatLiteralType, NotANumber);
    case JXC_ENUMSTR(FloatLiteralType, PosInfinity);
    case JXC_ENUMSTR(FloatLiteralType, NegInfinity);
    default: break;
    }
    return "INVALID";
}


template<size_t BufSize>
static inline void char_buffer_write_char(detail::MiniBuffer<char, BufSize>& out_buf, char value, size_t& inout_index)
{
    JXC_ASSERT(inout_index < out_buf.capacity());
    out_buf[inout_index] = value;
    ++inout_index;
}


template<uint32_t MinNumDigits, uint32_t MaxNumDigits, typename IntType, size_t BufSize>
static void char_buffer_write_integer(detail::MiniBuffer<char, BufSize>& out_buf, IntType value, size_t& inout_index)
{
    static_assert(std::is_integral_v<IntType>, "write_value only supports integer types");
    static_assert(BufSize >= MaxNumDigits, "output buffer too small to hold even one value of this size");

    JXC_ASSERT(inout_index + MaxNumDigits < BufSize);

    // we only write unsigned values, but we only need to actually check for signed types
    if constexpr (std::is_signed_v<IntType>)
    {
        // flip to positive and write it without any sign chars
        if (value < 0)
        {
            value = -value;
        }
    }

    char* start = &out_buf[inout_index];
    char* end = nullptr;

    // count the number of digits we need to write
    uint32_t num_digits_in_value = 0;

    if (value == 0)
    {
        num_digits_in_value = 1;
    }
    else
    {
        uint64_t val = static_cast<uint64_t>(value);
        while (val > 0)
        {
            ++num_digits_in_value;
            val /= 10;
        }
    }

    JXC_DEBUG_ASSERT(num_digits_in_value != 0);

    // clamp number of digits to the maximum (we assume validation occurs before this is called)
    JXC_DEBUG_ASSERT(num_digits_in_value <= MaxNumDigits);
    if (num_digits_in_value > MaxNumDigits)
    {
        num_digits_in_value = MaxNumDigits;
    }

    size_t num_padding_zeroes = 0;
    uint32_t target_num_digits_to_write = num_digits_in_value;
    if (MinNumDigits > target_num_digits_to_write)
    {
        target_num_digits_to_write = MinNumDigits;
        num_padding_zeroes = MinNumDigits - num_digits_in_value;
    }

    const size_t end_index = inout_index + target_num_digits_to_write;
    JXC_DEBUG_ASSERTF(end_index < BufSize, "Expected end_index={} to be valid for a buffer of size {}", end_index, BufSize);

    // pointer to one char AFTER the last valid char, because that's what std::to_chars wants
    end = (char*)((&out_buf[end_index]) + 1);
    JXC_DEBUG_ASSERT(start < end);

    size_t num_digits_written = 0;

    // add zero padding
    while (num_digits_written < num_padding_zeroes && inout_index < out_buf.capacity())
    {
        out_buf[inout_index] = '0';
        ++inout_index;
        ++start;
        ++num_digits_written;
    }

    JXC_DEBUG_ASSERT(start < end);
    JXC_DEBUG_ASSERT(static_cast<int64_t>(end - start) >= num_digits_in_value);

    const std::to_chars_result chars_result = std::to_chars(start, end, value);
    JXC_DEBUG_ASSERT(chars_result.ec == std::errc{ 0 });
    if (chars_result.ec == std::errc{ 0 })
    {
        const size_t num_chars_written = static_cast<size_t>(chars_result.ptr - start);
        inout_index += num_chars_written;
    }
}


std::string date_to_iso8601(const Date& dt)
{
    detail::MiniBuffer<char, 32> date_buf;
    size_t buf_index = 0;
    if (dt.year < 0)
    {
        // handle negative year values
        char_buffer_write_char(date_buf, '-', buf_index);
        char_buffer_write_integer<4, 6>(date_buf, -dt.year, buf_index);
    }
    else
    {
        char_buffer_write_integer<4, 6>(date_buf, dt.year, buf_index);
    }
    char_buffer_write_char(date_buf, '-', buf_index);
    char_buffer_write_integer<2, 2>(date_buf, dt.month, buf_index);
    char_buffer_write_char(date_buf, '-', buf_index);
    char_buffer_write_integer<2, 2>(date_buf, dt.day, buf_index);
    return std::string(date_buf.as_view(buf_index));
}


std::string datetime_to_iso8601(const DateTime& dt, bool auto_strip_time)
{
    detail::MiniBuffer<char, 64> datetime_buf;
    const bool is_local = dt.is_timezone_local();
    const bool is_utc = dt.is_timezone_utc();
    size_t buf_index = 0;

    // date
    if (dt.year < 0)
    {
        // handle negative year values
        char_buffer_write_char(datetime_buf, '-', buf_index);
        char_buffer_write_integer<4, 6>(datetime_buf, -dt.year, buf_index);
    }
    else
    {
        char_buffer_write_integer<4, 6>(datetime_buf, dt.year, buf_index);
    }
    char_buffer_write_char(datetime_buf, '-', buf_index);
    char_buffer_write_integer<2, 2>(datetime_buf, dt.month, buf_index);
    char_buffer_write_char(datetime_buf, '-', buf_index);
    char_buffer_write_integer<2, 2>(datetime_buf, dt.day, buf_index);

    if (!auto_strip_time || dt.has_time_or_timezone_data())
    {
        // time sep
        char_buffer_write_char(datetime_buf, 'T', buf_index);

        // time
        char_buffer_write_integer<2, 2>(datetime_buf, dt.hour, buf_index);
        char_buffer_write_char(datetime_buf, ':', buf_index);
        char_buffer_write_integer<2, 2>(datetime_buf, dt.minute, buf_index);
        char_buffer_write_char(datetime_buf, ':', buf_index);
        char_buffer_write_integer<2, 2>(datetime_buf, dt.second, buf_index);

        if (dt.nanosecond > 0)
        {
            char_buffer_write_char(datetime_buf, '.', buf_index);

            if ((dt.nanosecond % 1000000) == 0)
            {
                // If the value in nanoseconds can be converted to milliseconds with no data loss, write out the value in milliseconds
                const uint32_t dt_milli = dt.nanosecond / 1000000;
                char_buffer_write_integer<3, 3>(datetime_buf, dt_milli, buf_index);
            }
            else if ((dt.nanosecond % 1000) == 0)
            {
                // If the value in nanoseconds can be converted to microseconds with no data loss, write out the value in microseconds
                const uint32_t dt_micro = dt.nanosecond / 1000;
                char_buffer_write_integer<6, 6>(datetime_buf, dt_micro, buf_index);
            }
            else
            {
                // Write the full nanosecond value
                char_buffer_write_integer<9, 9>(datetime_buf, dt.nanosecond, buf_index);
            }
        }

        // time zone
        if (is_local)
        {
            // for local times, don't write time zone info
        }
        else if (is_utc)
        {
            char_buffer_write_char(datetime_buf, 'Z', buf_index);
        }
        else
        {
            if (dt.tz_hour < 0)
            {
                char_buffer_write_char(datetime_buf, '-', buf_index);
                char_buffer_write_integer<2, 2>(datetime_buf, -dt.tz_hour, buf_index);
            }
            else
            {
                char_buffer_write_char(datetime_buf, '+', buf_index);
                char_buffer_write_integer<2, 2>(datetime_buf, dt.tz_hour, buf_index);
            }

            char_buffer_write_char(datetime_buf, ':', buf_index);
            char_buffer_write_integer<2, 2>(datetime_buf, dt.tz_minute, buf_index);
        }
    }

    return std::string(datetime_buf.as_view(buf_index));
}


const char* token_type_to_string(TokenType type)
{
    switch (type)
    {
    case JXC_ENUMSTR(TokenType, Invalid);
    case JXC_ENUMSTR(TokenType, Comment);
    case JXC_ENUMSTR(TokenType, Identifier);
    case JXC_ENUMSTR(TokenType, True);
    case JXC_ENUMSTR(TokenType, False);
    case JXC_ENUMSTR(TokenType, Null);
    case JXC_ENUMSTR(TokenType, Number);
    case JXC_ENUMSTR(TokenType, String);
    case JXC_ENUMSTR(TokenType, ByteString);
    case JXC_ENUMSTR(TokenType, DateTime);
    case JXC_ENUMSTR(TokenType, Colon);
    case JXC_ENUMSTR(TokenType, Equals);
    case JXC_ENUMSTR(TokenType, Comma);
    case JXC_ENUMSTR(TokenType, Period);
    case JXC_ENUMSTR(TokenType, BraceOpen);
    case JXC_ENUMSTR(TokenType, BraceClose);
    case JXC_ENUMSTR(TokenType, SquareBracketOpen);
    case JXC_ENUMSTR(TokenType, SquareBracketClose);
    case JXC_ENUMSTR(TokenType, AngleBracketOpen);
    case JXC_ENUMSTR(TokenType, AngleBracketClose);
    case JXC_ENUMSTR(TokenType, ParenOpen);
    case JXC_ENUMSTR(TokenType, ParenClose);
    case JXC_ENUMSTR(TokenType, ExclamationPoint);
    case JXC_ENUMSTR(TokenType, Asterisk);
    case JXC_ENUMSTR(TokenType, QuestionMark);
    case JXC_ENUMSTR(TokenType, AtSymbol);
    case JXC_ENUMSTR(TokenType, Pipe);
    case JXC_ENUMSTR(TokenType, Ampersand);
    case JXC_ENUMSTR(TokenType, Percent);
    case JXC_ENUMSTR(TokenType, Semicolon);
    case JXC_ENUMSTR(TokenType, Plus);
    case JXC_ENUMSTR(TokenType, Minus);
    case JXC_ENUMSTR(TokenType, Slash);
    case JXC_ENUMSTR(TokenType, Backslash);
    case JXC_ENUMSTR(TokenType, Caret);
    case JXC_ENUMSTR(TokenType, Tilde);
    case JXC_ENUMSTR(TokenType, Backtick);
    case JXC_ENUMSTR(TokenType, LineBreak);
    case JXC_ENUMSTR(TokenType, EndOfStream);
    case TokenType::COUNT:
        break;
    }
    return "UnknownTokenType";
}


const char* token_type_to_symbol(TokenType type)
{
    switch (type)
    {
    //case TokenType::Comment: break;
    //case TokenType::Identifier: break;
    case TokenType::True: return "true";
    case TokenType::False: return "false";
    case TokenType::Null: return "null";
    //case TokenType::Number: break;
    //case TokenType::String: break;
    //case TokenType::DateTime: break;
    //case TokenType::ByteString: break;
    case TokenType::Colon: return ":";
    case TokenType::Equals: return "=";
    case TokenType::Comma: return ",";
    case TokenType::Period: return ".";
    case TokenType::BraceOpen: return "{";
    case TokenType::BraceClose: return "}";
    case TokenType::SquareBracketOpen: return "[";
    case TokenType::SquareBracketClose: return "]";
    case TokenType::AngleBracketOpen: return "<";
    case TokenType::AngleBracketClose: return ">";
    case TokenType::ParenOpen: return "(";
    case TokenType::ParenClose: return ")";
    case TokenType::ExclamationPoint: return "!";
    case TokenType::Asterisk: return "*";
    case TokenType::QuestionMark: return "?";
    case TokenType::AtSymbol: return "@";
    case TokenType::Pipe: return "|";
    case TokenType::Ampersand: return "&";
    case TokenType::Percent: return "%";
    case TokenType::Semicolon: return ";";
    case TokenType::Plus: return "+";
    case TokenType::Minus: return "-";
    case TokenType::Slash: return "/";
    case TokenType::Backslash: return "\\";
    case TokenType::Caret: return "^";
    case TokenType::Tilde: return "~";
    case TokenType::Backtick: return "`";
    case TokenType::LineBreak: return "\n";
    //case TokenType::EndOfStream: break;
    default: break;
    }
    return "";
}


TokenType token_type_from_symbol(std::string_view sym)
{
    const size_t sz = sym.size();
    auto is_quote_char = [](char ch) { return ch == '\'' || ch == '\"'; };
    switch (sz)
    {
    case 1:
        switch (sym[0])
        {
        case '#': return TokenType::Comment;
        //case TokenType::Identifier: break;
        //case TokenType::True: return "true";
        //case TokenType::False: return "false";
        //case TokenType::Null: return "null";
        //case TokenType::Number: break;
        //case TokenType::String: break;
        //case TokenType::DateTime: break;
        //case TokenType::ByteString: break;
        case ':': return TokenType::Colon;
        case '=': return TokenType::Equals;
        case ',': return TokenType::Comma;
        case '.': return TokenType::Period;
        case '{': return TokenType::BraceOpen;
        case '}': return TokenType::BraceClose;
        case '[': return TokenType::SquareBracketOpen;
        case ']': return TokenType::SquareBracketClose;
        case '<': return TokenType::AngleBracketOpen;
        case '>': return TokenType::AngleBracketClose;
        case '(': return TokenType::ParenOpen;
        case ')': return TokenType::ParenClose;
        case '!': return TokenType::ExclamationPoint;
        case '*': return TokenType::Asterisk;
        case '?': return TokenType::QuestionMark;
        case '@': return TokenType::AtSymbol;
        case '|': return TokenType::Pipe;
        case '&': return TokenType::Ampersand;
        case '%': return TokenType::Percent;
        case ';': return TokenType::Semicolon;
        case '+': return TokenType::Plus;
        case '-': return TokenType::Minus;
        case '/': return TokenType::Slash;
        case '\\': return TokenType::Backslash;
        case '^': return TokenType::Caret;
        case '~': return TokenType::Tilde;
        case '`': return TokenType::Backtick;
        case '\n': return TokenType::LineBreak;
        default: break;
        }
        break;
    case 4:
        if (sym[0] == 't' && sym[1] == 'r' && sym[2] == 'u' && sym[3] == 'e')
        {
            return TokenType::True;
        }
        else if (sym[0] == 'n' && sym[1] == 'u' && sym[2] == 'l' && sym[3] == 'l')
        {
            return TokenType::Null;
        }
        break;
    case 5:
        if (sym[0] == 'f' && sym[1] == 'a' && sym[2] == 'l' && sym[3] == 's' && sym[4] == 'e')
        {
            return TokenType::False;
        }
        break;
    default:
        break;
    }

    if (sz >= 2 && sym[0] == sym.back() && is_quote_char(sym[0]))
    {
        return TokenType::String;
    }
    else if (sz >= 5 && sym[0] == 'b' && sym[1] == '6' && sym[2] == '4' && sym[3] == sym.back() && is_quote_char(sym[3]))
    {
        return TokenType::ByteString;
    }
    else if (sz >= 4 && sym[0] == 'd' && sym[1] == 't' && sym[2] == sym.back() && is_quote_char(sym[2]))
    {
        return TokenType::DateTime;
    }
    else if (is_valid_identifier(sym) || is_valid_object_key(sym))
    {
        return TokenType::Identifier;
    }

    return TokenType::Invalid;
}


FlexString detail::concat_token_values(const Token* first_token, size_t token_count)
{
    if (first_token == nullptr || token_count == 0)
    {
        return FlexString{};
    }

    std::vector<char> result;
    result.reserve(token_count * 2);

    auto append_char = [&result](char ch)
    {
        result.push_back(ch);
    };

    auto append_string = [&result](std::string_view str)
    {
        size_t idx = result.size();
        result.resize(result.size() + str.size());
        JXC_MEMCPY(&result[idx], str.size(), str.data(), str.size());
    };

    TokenType prev_tok_type = TokenType::Invalid;
    for (size_t i = 0; i < token_count; i++)
    {
        const Token& tok = first_token[i];
        if (i > 0)
        {
            // put a space between the prev token and this one?
            switch (prev_tok_type)
            {
            // always sep
            case TokenType::True:
            case TokenType::False:
            case TokenType::Null:
            case TokenType::Number:
            case TokenType::String:
            case TokenType::ByteString:
            case TokenType::DateTime:
            case TokenType::Colon:
            case TokenType::Comma:
                append_char(' ');
                break;

            // sep only when the next token has a value
            case TokenType::Identifier:
                if (tok.type != TokenType::Colon && tok.type != TokenType::Period && !token_type_has_value(tok.type))
                {
                    append_char(' ');
                }
                break;

            default:
                break;
            }
        }

        if (token_type_has_value(tok.type))
        {
            append_string(tok.value.as_view());
        }
        else
        {
            append_string(token_type_to_symbol(tok.type));
        }

        prev_tok_type = tok.type;
    }

    return FlexString(std::string_view{ result.data(), result.size() });
}


bool Token::get_line_and_col(std::string_view buf, size_t& out_line, size_t& out_col) const
{
    return detail::find_line_and_col(buf, start_idx, out_line, out_col);
}


std::string Token::to_repr() const
{
    std::string result = jxc::format("Token.{}(", jxc::token_type_to_string(type));
    if (value.size() > 0)
    {
        result.append(jxc::detail::debug_string_repr(value.as_view()));
    }
    else
    {
        result.append(jxc::token_type_to_symbol(type));
    }

    if (tag.size() > 0)
    {
        result.append(jxc::format(", tag={}", jxc::detail::debug_string_repr(tag.as_view())));
    }

    result.push_back(')');
    return result;
}


std::string Token::to_string() const
{
    return (token_type_has_value(type) && value.size() > 0) ? std::string(value.as_view()) : std::string(token_type_to_symbol(type));
}


TokenSpan::TokenSpan(const OwnedTokenSpan& rhs)
    : start(const_cast<Token*>(rhs.tokens.data()))
    , num_tokens(rhs.size())
{
}


bool TokenSpan::operator==(const OwnedTokenSpan& rhs) const
{
    return *this == rhs.to_token_span();
}


bool TokenSpan::operator==(std::string_view rhs) const
{
    // this function must behave exactly the same as comparing against a TokenSpan with exactly zero or one token.
    const size_t sz = size();
    if (rhs.size() == 0)
    {
        return sz == 0;
    }
    else if (sz > 1)
    {
        // string can only be one token, so this can't be equal
        return false;
    }

    // size() will return 0 if start is nullptr - it's safe to deref here
    return start->type == token_type_from_symbol(rhs) && (!token_type_has_value(start->type) || start->value.as_view() == rhs);
}


bool TokenSpan::equals_annotation_string_lexed(std::string_view str) const
{
    if (num_tokens == 0 || str.size() == 0)
    {
        return num_tokens == 0 && str.size() == 0;
    }

    size_t tok_idx = 0;
    AnnotationLexer lex{ str };
    Token tok;
    while (lex.next(tok))
    {
        if (tok_idx >= num_tokens || tok != start[tok_idx])
        {
            return false;
        }
        ++tok_idx;
    }
    return tok_idx == num_tokens;
}


std::string TokenSpan::to_repr() const
{
    std::ostringstream ss;
    ss << "TokenSpan(";
    bool first = true;
    for (size_t i = 0; i < num_tokens; i++)
    {
        if (!first)
        {
            ss << ", ";
        }
        ss << start[i].to_repr();
        first = false;
    }
    ss << ")";
    return ss.str();
}


std::string TokenSpan::to_string() const
{
    std::ostringstream ss;
    for (size_t i = 0; i < num_tokens; i++)
    {
        ss << start[i].to_string();
    }
    return ss.str();
}


uint64_t TokenSpan::hash() const
{
    uint64_t result = 0;
    const size_t sz = (start != nullptr) ? num_tokens : 0;
    detail::hash_combine<uint64_t>(result, sz);
    for (size_t i = 0; i < sz; i++)
    {
        const Token& tok = start[i];
        detail::hash_combine<uint64_t>(result, static_cast<uint64_t>(tok.type));
        if (token_type_has_value(tok.type))
        {
            detail::hash_combine<uint64_t>(result, std::hash<std::string_view>()(tok.value.as_view()));
        }
    }
    return result;
}


//static
uint64_t TokenSpan::hash_string_as_single_token(std::string_view str, TokenType tok_type)
{
    // this function must behave exactly the same as the above TokenSpan::hash() method, assuming that token span has exactly zero or one token.
    uint64_t result = 0;
    const size_t token_count = (str.size() > 0) ? 1 : 0;
    detail::hash_combine<uint64_t>(result, token_count);

    if (token_count > 0)
    {
        if (tok_type == TokenType::Invalid)
        {
            tok_type = token_type_from_symbol(str);
        }
        detail::hash_combine<uint64_t>(result, static_cast<uint64_t>(tok_type));
        if (token_type_has_value(tok_type))
        {
            detail::hash_combine<uint64_t>(result, std::hash<std::string_view>()(str));
        }
    }
    return result;
}


FlexString TokenSpan::source(bool force_owned) const
{
    if (start == nullptr || num_tokens == 0)
    {
        return FlexString();
    }
    
    // we can fake a view when we have exactly one token and it's a non-value type (token_type_to_symbol returns a static string)
    if (start != nullptr && num_tokens == 1 && !token_type_has_value(start->type)
        && (start->type != TokenType::Invalid && start->type != TokenType::EndOfStream))
    {
        // safe to ignore force_owned here
        return FlexString::make_view(token_type_to_symbol(start->type));
    }

    // we can only make a string_view representing the original source if all tokens are views
    bool can_make_view = true;
    for (size_t i = 0; i < num_tokens; i++)
    {
        if (!start[i].value.is_view())
        {
            can_make_view = false;
            break;
        }
    }

    if (can_make_view)
    {
        // TODO: This assumes that all of the token value string views are contiguous. Determine if this is a problem.
        std::string_view end_view = start[num_tokens - 1].value.as_view();
        const char* ptr_start = start->value.data();
        std::string_view result{ ptr_start, static_cast<size_t>((end_view.data() + end_view.size()) - ptr_start) };
        return force_owned ? FlexString::make_owned(result) : FlexString::make_view(result);
    }

    // not a great fallback because we don't have the original whitespace data, but it's better than nothing
    return detail::concat_token_values(start, num_tokens);
}


const char* string_quote_mode_to_string(StringQuoteMode mode)
{
    switch (mode)
    {
    case JXC_ENUMSTR(StringQuoteMode, Auto);
    case JXC_ENUMSTR(StringQuoteMode, Double);
    case JXC_ENUMSTR(StringQuoteMode, Single);
    default: break;
    }
    return "INVALID";
}


std::string SerializerSettings::to_repr() const
{
    std::ostringstream ss;

    int32_t num_fields = 0;

    auto add_field = [&](std::string&& field)
    {
        if (num_fields > 0)
        {
            ss << ", ";
        }
        ss << field;
        ++num_fields;
    };

    add_field(jxc::format("pretty_print={}", detail::debug_bool_repr(pretty_print)));
    add_field(jxc::format("target_line_length={}", target_line_length));
    add_field(jxc::format("indent={}", detail::debug_string_repr(indent)));
    add_field(jxc::format("linebreak={}", detail::debug_string_repr(linebreak)));
    add_field(jxc::format("key_separator={}", detail::debug_string_repr(key_separator)));
    add_field(jxc::format("value_separator={}", detail::debug_string_repr(value_separator)));

    return jxc::format("SerializerSettings({})", ss.str());
}


namespace base64
{
    bool is_base64_char(char ch)
    {
        switch (ch)
        {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
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
        case '+':
        case '/':
        case '=':
            return true;
        default:
            break;
        }
        return false;
    }

    // adapted from https://gist.github.com/tomykaira/f0fd86b6c73063283afe550bc5d77594
    // (MIT license, (C) 2016 tomykaira)

    static constexpr char encoding_table[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'
    };

    static constexpr unsigned char decoding_table[] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };

    size_t get_num_bytes_in_base64_multiline_string(const char* base64_str, size_t base64_str_len)
    {
        if (base64_str == nullptr || base64_str_len == 0)
        {
            return 0;
        }

        // tiny buffer to store the last four base64 chars so we can check if they're padding at the end
        ::jxc::detail::MiniBuffer<char, 4> buf;
        size_t buf_idx = 0;

        size_t num_base64_chars = 0;
        for (size_t i = 0; i < base64_str_len; i++)
        {
            if (is_base64_char(base64_str[i]))
            {
                ++num_base64_chars;

                buf[buf_idx] = base64_str[i];
                ++buf_idx;
                if (buf_idx >= buf.capacity())
                {
                    buf_idx = 0;
                }
            }
        }

        if (num_base64_chars % 4 != 0)
        {
            // Input data size is not a multiple of 4
            return 0;
        }

        size_t num_bytes = num_base64_chars / 4 * 3;

        for (int i = (int)buf.capacity() - 1; i >= 0; --i)
        {
            if (buf[i] == '=')
            {
                --num_bytes;
            }
            else
            {
                break;
            }
        }

        return num_bytes;
    }

    void bytes_to_base64(const uint8_t* bytes, size_t bytes_len, char* out_data, size_t out_size)
    {
        const size_t req_out_size = get_base64_string_size(bytes_len);
        JXC_ASSERTF(out_size >= req_out_size,
            "bytes_to_base64 requires {} bytes, but out_size is {}", req_out_size, out_size);

        size_t i = 0;
        char* p = out_data;

        for (i = 0; i < bytes_len - 2; i += 3)
        {
            *p++ = encoding_table[(bytes[i] >> 2) & 0x3F];
            *p++ = encoding_table[((bytes[i] & 0x3) << 4) | ((int)(bytes[i + 1] & 0xF0) >> 4)];
            *p++ = encoding_table[((bytes[i + 1] & 0xF) << 2) | ((int)(bytes[i + 2] & 0xC0) >> 6)];
            *p++ = encoding_table[bytes[i + 2] & 0x3F];
        }

        if (i < bytes_len)
        {
            *p++ = encoding_table[(bytes[i] >> 2) & 0x3F];
            if (i == (bytes_len - 1))
            {
                *p++ = encoding_table[((bytes[i] & 0x3) << 4)];
                *p++ = '=';
            }
            else
            {
                *p++ = encoding_table[((bytes[i] & 0x3) << 4) | ((int)(bytes[i + 1] & 0xF0) >> 4)];
                *p++ = encoding_table[((bytes[i + 1] & 0xF) << 2)];
            }
            *p++ = '=';
        }
    }


    void base64_to_bytes(const char* base64_str, size_t base64_str_len, uint8_t* out_data, size_t out_size)
    {
        const size_t req_out_size = get_num_bytes_in_base64_string(base64_str, base64_str_len);
        JXC_ASSERTF(out_size >= req_out_size,
            "base64_to_bytes requires {} bytes, but out_size is {}", req_out_size, out_size);

        size_t out_data_idx = 0;
        const size_t str_len = base64_str_len;
        for (size_t i = 0; i < str_len;)
        {
            const uint32_t a = (base64_str[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(base64_str[i])];
            ++i;

            const uint32_t b = (base64_str[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(base64_str[i])];
            ++i;

            const uint32_t c = (base64_str[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(base64_str[i])];
            ++i;

            const uint32_t d = (base64_str[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(base64_str[i])];
            ++i;

            const uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

            if (out_data_idx < out_size)
            {
                out_data[out_data_idx++] = (triple >> 2 * 8) & 0xFF;
            }

            if (out_data_idx < out_size)
            {
                out_data[out_data_idx++] = (triple >> 1 * 8) & 0xFF;
            }

            if (out_data_idx < out_size)
            {
                out_data[out_data_idx++] = (triple >> 0 * 8) & 0xFF;
            }
        }
    }


    size_t base64_multiline_to_bytes(const char* base64_str, size_t base64_str_len, uint8_t* out_data, size_t out_size)
    {
        JXC_ASSERT(base64_str != nullptr && base64_str_len >= 4);
        size_t out_data_idx = 0;
        detail::MiniBuffer<char, 4> quad;

#define JXC_B64_FASTFORWARD(STR_IDX) do { \
    while (STR_IDX < base64_str_len && !is_base64_char(base64_str[STR_IDX])) { \
        ++STR_IDX; \
    } \
} while(0)

        size_t str_idx = 0;
        while (str_idx < base64_str_len && out_data_idx < out_size)
        {
            JXC_B64_FASTFORWARD(str_idx);
            if (str_idx >= base64_str_len)
            {
                break;
            }

            quad[0] = base64_str[str_idx++];

            JXC_B64_FASTFORWARD(str_idx);
            if (str_idx >= base64_str_len)
            {
                // error - stopped in the middle of a quad
                return 0;
            }

            quad[1] = base64_str[str_idx++];

            JXC_B64_FASTFORWARD(str_idx);
            if (str_idx >= base64_str_len)
            {
                // error - stopped in the middle of a quad
                return 0;
            }

            quad[2] = base64_str[str_idx++];

            JXC_B64_FASTFORWARD(str_idx);
            if (str_idx >= base64_str_len)
            {
                // error - stopped in the middle of a quad
                return 0;
            }

            quad[3] = base64_str[str_idx++];

            // push quad to output
            {
                int i = 0;

                const uint32_t a = (quad[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(quad[i])];
                ++i;

                const uint32_t b = (quad[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(quad[i])];
                ++i;

                const uint32_t c = (quad[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(quad[i])];
                ++i;

                const uint32_t d = (quad[i] == '=') ? (0 & i) : decoding_table[static_cast<int>(quad[i])];

                const uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

                if (out_data_idx < out_size)
                {
                    out_data[out_data_idx++] = (triple >> 2 * 8) & 0xFF;
                }

                if (out_data_idx < out_size)
                {
                    out_data[out_data_idx++] = (triple >> 1 * 8) & 0xFF;
                }

                if (out_data_idx < out_size)
                {
                    out_data[out_data_idx++] = (triple >> 0 * 8) & 0xFF;
                }
            }
        }

#undef JXC_B64_FASTFORWARD

        return out_data_idx;
    }

} // namespace base64


template<typename LexerType>
static inline std::optional<OwnedTokenSpan> parse_internal(std::string_view source, std::string* out_error)
{
    OwnedTokenSpan result;

    if (source.size() == 0)
    {
        return result;
    }

    result.src = FlexString(source);

    LexerType lex(source);

    Token tok;
    while (lex.next(tok))
    {
        result.tokens.push(tok);
    }

    if (lex.has_error())
    {
        if (out_error != nullptr)
        {
            *out_error = lex.get_error_message();
        }
        return std::nullopt;
    }

    return result;
}


//static
std::optional<OwnedTokenSpan> OwnedTokenSpan::parse(std::string_view source, std::string* out_error)
{
    return parse_internal<TokenLexer>(source, out_error);
}


//static
std::optional<OwnedTokenSpan> OwnedTokenSpan::parse_annotation(std::string_view annotation, std::string* out_error)
{
    return parse_internal<AnnotationLexer>(annotation, out_error);
}


//static
std::optional<OwnedTokenSpan> OwnedTokenSpan::parse_expression(std::string_view expression, std::string* out_error)
{
    return parse_internal<ExpressionLexer>(expression, out_error);
}


FlexString OwnedTokenSpan::source(bool force_owned) const
{
    if (src.size() > 0)
    {
        return force_owned ? FlexString::make_owned(src.as_view()) : src;
    }
    else if (tokens.size() == 0)
    {
        return FlexString();
    }
    else
    {
        return detail::concat_token_values(&tokens.front(), tokens.size());
    }
}


} // namespace jxc
