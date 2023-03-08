#pragma once
#include <stdint.h>
#include <limits>
#include <string>
#include <string_view>
#include <functional>
#include <chrono>
#include <array>
#include <optional>

#include "jxc/jxc_core.h"
#include "jxc/jxc_string.h"
#include "jxc/jxc_bytes.h"
#include "jxc/jxc_array.h"


JXC_BEGIN_NAMESPACE(jxc)

class Serializer;

JXC_BEGIN_NAMESPACE(detail)

// Gets the base filename (with no path info) of a path. Returns a string_view into the string passed into the function.
// Intended for shortening the __FILE__ macro for use in error messages.
std::string_view get_base_filename(const char* file);

std::optional<std::string> read_file_to_string(const std::string& file_path, std::string* out_error = nullptr);

std::string debug_string_repr(std::string_view value, char quote_char = '"');

std::string debug_bytes_repr(BytesView bytes, char quote_char = '"');

// Returns all the input bytes as a string, except all non-printable character bytes are encoded in the form `\xFF`
std::string encode_bytes_to_string(BytesView bytes);

inline const char* debug_bool_repr(bool value) { return value ? "true" : "false"; }

// intended for debug messages - always returns a printable version of the character (in hex form if needed)
std::string debug_char_repr(uint32_t codepoint, char quote_char = '`');

bool is_ascii_escape_char(uint32_t codepoint, char quote_char = '\0');

inline bool is_renderable_ascii_char(uint32_t codepoint) { return codepoint >= 32 && codepoint <= 126; }

// Serializes an ASCII character. Requires a minimum buffer length of 4 for the largest escape form.
size_t serialize_ascii_codepoint(uint8_t codepoint, char* out_buf, size_t out_buf_len,
    bool escape_backslash = false, bool escape_single_quote = false, bool escape_double_quote = false);

// Serializes an ASCII character. Requires a minimum buffer length of 10 for the largest escape form.
size_t serialize_utf32_codepoint(uint32_t codepoint, char* out_buf, size_t out_buf_len);

// Deserializes hex chars (eg. "6a" or "1f600").
uint32_t deserialize_hex_to_codepoint(const char* hex_buf, size_t hex_buf_len, std::string* out_error = nullptr);

template<typename T>
inline void hash_combine(T& inout_hash, T new_hash_value)
{
    static_assert(std::is_integral_v<T>, "hash_combine requires an int type");
    inout_hash ^= new_hash_value + 0x9e3779b9 + (inout_hash << 6) + (inout_hash >> 2);
}

template<typename T>
constexpr inline T hash_combine2(T orig_hash, T value)
{
    static_assert(std::is_integral_v<T>, "hash_combine requires an int type");
    return orig_hash ^ (value + 0x9e3779b9 + (orig_hash << 6) + (orig_hash >> 2));
}

// simple benchmarking tool
struct Timer
{
    std::chrono::high_resolution_clock::time_point start;
    Timer() { reset(); }
    inline void reset() { start = std::chrono::high_resolution_clock::now(); }
    inline std::chrono::nanoseconds elapsed() const { return std::chrono::high_resolution_clock::now() - start; }

    static inline double ns_to_ms(int64_t time_ns) { return (double)time_ns / 1e6; }
};

JXC_BEGIN_NAMESPACE(utf8)

// utf8 functions adapted from https://gist.github.com/tylerneylon/9773800 (public domain code)

static constexpr uint32_t error_char = 0xFFFD;

static constexpr uint32_t max_1byte_codepoint = 0x7f;
static constexpr uint32_t max_2byte_codepoint = 0x7ff;
static constexpr uint32_t max_3byte_codepoint = 0xffff;
static constexpr uint32_t max_4byte_codepoint = 0x1fffff;

// Returns the number of bytes needed to encode a utf32 codepoint
int32_t num_codepoint_bytes(uint32_t codepoint);

// Decodes a stream of utf8 bytes to utf32 codepoints
uint32_t decode(const char* buf, size_t buf_len, size_t& inout_index);

// Encodes a utf32 codepoint to a stream of utf8 bytes
// Assumes that code is <= 0x10FFFF. Ensures that nothing will be written at or beyond end.
// Returns the number of characters written.
size_t encode(char* buf, size_t buf_len, size_t& inout_index, uint32_t codepoint);

// Returns false if no split was needed.
bool split_into_surrogates(uint32_t codepoint, uint32_t& out_surr1, uint32_t& out_surr2);

// Expects to be used in a loop and see all code points in out_codepoint. Start inout_old at 0;
// this function updates inout_old for you - don't change it. Returns true when out_codepoint is
// the 1st of a surrogate pair; otherwise use out_codepoint as the final code point.
bool join_from_surrogates(uint32_t& inout_old, uint32_t& out_codepoint);

JXC_END_NAMESPACE(utf8)

JXC_END_NAMESPACE(detail)


// Checks if a character is valid as the first character of an identifier
inline bool is_valid_identifier_first_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '$';
}


// Checks if a character is valid in an identifier (as any character other than the first)
inline bool is_valid_identifier_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$';
}


// Checks if a given string is a valid identifier
bool is_valid_identifier(std::string_view value);


// Checks if a given string is a valid identifier for an object key (allows dots as separators)
bool is_valid_object_key(std::string_view key, bool allow_separators = true);

// Classifier for JXC float literals (nan, +inf, -inf)
enum class FloatLiteralType : uint8_t
{
    Finite = 0,
    NotANumber,
    PosInfinity,
    NegInfinity,
};

const char* float_literal_type_to_string(FloatLiteralType type);

inline std::ostream& operator<<(std::ostream& os, FloatLiteralType val)
{
    return (os << float_literal_type_to_string(val));
}

template<typename T>
inline FloatLiteralType get_float_literal_type(T value)
{
    static_assert(std::is_floating_point_v<T>, "get_float_literal_type requires a floating point type");
    if (std::isfinite(value))
    {
        return FloatLiteralType::Finite;
    }
    else if (std::isinf(value))
    {
        return (value < 0) ? FloatLiteralType::NegInfinity : FloatLiteralType::PosInfinity;
    }

    JXC_DEBUG_ASSERT(std::isnan(value));
    return FloatLiteralType::NotANumber;
}

// Converts a Date to ISO-8601
std::string date_to_iso8601(const Date& dt);

// Converts a DateTime to ISO-8601
// If auto_strip_time is true, and the DateTime has no time data (see DateTime::has_time_or_timezone_data), this is serialized as just a Date.
std::string datetime_to_iso8601(const DateTime& dt, bool auto_strip_time = false);

inline std::ostream& operator<<(std::ostream& os, const Date& dt)
{
    return (os << date_to_iso8601(dt));
}

inline std::ostream& operator<<(std::ostream& os, const DateTime& dt)
{
    return (os << datetime_to_iso8601(dt));
}

// JXC token types
enum class TokenType : uint8_t
{
    Invalid = 0,
    Comment,

    // value tokens
    Identifier,
    True,
    False,
    Null,
    Number,
    String,
    ByteString,
    DateTime,

    // syntax characters
    Colon,
    Equals,
    Comma,
    Period,
    BraceOpen,
    BraceClose,
    SquareBracketOpen,
    SquareBracketClose,
    AngleBracketOpen,
    AngleBracketClose,
    ParenOpen,
    ParenClose,
    ExclamationPoint,
    Asterisk,
    QuestionMark,
    AtSymbol,
    Pipe,
    Ampersand,
    Percent,
    Semicolon,
    Plus,
    Minus,
    Slash,
    Backslash,
    Caret,
    Tilde,
    Backtick,

    // misc
    LineBreak,
    EndOfStream,

    COUNT,
};


const char* token_type_to_string(TokenType type);


inline std::ostream& operator<<(std::ostream& os, TokenType type)
{
    return (os << token_type_to_string(type));
}


const char* token_type_to_symbol(TokenType type);


TokenType token_type_from_symbol(std::string_view symbol, bool allow_object_key = true);


inline TokenType token_type_from_symbol(char symbol)
{
    char buf[2] = { symbol, '\0' };
    return token_type_from_symbol(std::string_view{ buf, 1 });
}


// Returns true if a given token type has an associated value
inline bool token_type_has_value(TokenType type)
{
    switch (type)
    {
    case TokenType::Comment:
    case TokenType::Identifier:
    case TokenType::Number:
    case TokenType::String:
    case TokenType::ByteString:
    case TokenType::DateTime:
        return true;
    default:
        break;
    }
    return false;
}


struct Token
{
    TokenType type = TokenType::Invalid;
    FlexString value;
    FlexString tag; // Value associated with a token - raw string heredoc, number suffix, etc.
    size_t start_idx = invalid_idx;
    size_t end_idx = invalid_idx;

    Token() = default;
    Token(TokenType type) : type(type) {}
    Token(TokenType type, size_t start_idx, size_t end_idx, const FlexString& value, const FlexString& tag = FlexString{})
        : type(type)
        , value(value)
        , tag(tag)
        , start_idx(start_idx)
        , end_idx(end_idx)
    {
    }

    inline bool operator==(const Token& rhs) const
    {
        if (type != rhs.type)
        {
            return false;
        }

        if (token_type_has_value(type) && value != rhs.value)
        {
            return false;
        }

        // tag values only need to be compared for number tokens - for strings we don't care if the heredocs are the same or not
        if (type == TokenType::Number && tag != rhs.tag)
        {
            return false;
        }

        return true;
    }

    inline bool operator!=(const Token& rhs) const { return !operator==(rhs); }

    inline bool is_owned() const { return !value.is_view() && !tag.is_view(); }

    inline void to_owned_inplace()
    {
        value.to_owned_inplace();
        tag.to_owned_inplace();
    }

    inline Token copy() const
    {
        return Token{ type, start_idx, end_idx, value.to_owned(), tag.to_owned() };
    }

    inline Token view() const
    {
        return Token{ type, start_idx, end_idx, FlexString::make_view(value.as_view()), FlexString::make_view(tag.as_view()) };
    }

    inline void reset()
    {
        type = TokenType::Invalid;
        value.reset();
        tag.reset();
        start_idx = invalid_idx;
        end_idx = invalid_idx;
    }

    bool get_line_and_col(std::string_view buf, size_t& out_line, size_t& out_col) const;

    std::string to_repr() const;
    std::string to_string() const;
};


struct TokenList;


namespace detail
{
FlexString concat_token_values(const Token* first_token, size_t token_count);
} // namespace detail


struct TokenView
{
    friend struct TokenList;

private:
    inline void check_index(size_t idx) const
    {
        JXC_ASSERT(start != nullptr);
        JXC_ASSERTF(idx < num_tokens, "Index {} out of range for TokenView with {} tokens", idx, num_tokens);
    }

public:
    Token* start = nullptr;
    size_t num_tokens = 0;
    std::string_view source_view;

    TokenView() = default;
    TokenView(std::nullptr_t) : start(nullptr), num_tokens(0) {}
    TokenView(Token& start, size_t num_tokens, std::string_view source_view = {}) : start(&start), num_tokens(num_tokens), source_view(source_view) {}

    TokenView(const TokenView& rhs) = default;
    TokenView& operator=(const TokenView& rhs) = default;

    TokenView(const TokenList& rhs);

    inline size_t size() const { return (start != nullptr) ? num_tokens : 0; }

    inline explicit operator bool() const { return start != nullptr && num_tokens > 0; }

    inline Token& operator[](size_t idx) { check_index(idx); return start[idx]; }
    inline const Token& operator[](size_t idx) const { check_index(idx); return start[idx]; }

    inline bool operator==(const TokenView& rhs) const
    {
        if (num_tokens != rhs.num_tokens)
        {
            return false;
        }

        if (num_tokens > 0)
        {
            JXC_DEBUG_ASSERT(start != nullptr && rhs.start != nullptr);
            for (size_t i = 0; i < num_tokens; i++)
            {
                if (start[i] != rhs.start[i])
                {
                    return false;
                }
            }
        }

        return true;
    }

    inline bool operator!=(const TokenView& rhs) const { return !operator==(rhs); }

    bool operator==(const TokenList& rhs) const;
    inline bool operator!=(const TokenList& rhs) const { return !operator==(rhs); }

    bool operator==(std::string_view rhs) const;
    inline bool operator!=(std::string_view rhs) const { return !operator==(rhs); }

    friend bool operator==(std::string_view lhs, const TokenView& rhs) { return rhs.operator==(lhs); }
    friend bool operator!=(std::string_view lhs, const TokenView& rhs) { return rhs.operator!=(lhs); }

    // Returns the start_idx value from the first token, and the end_idx value from the last token.
    // Useful for error messages.
    std::pair<size_t, size_t> get_index_span() const
    {
        if (num_tokens > 0 && start != nullptr)
        {
            return std::make_pair(start->start_idx, operator[](num_tokens - 1).end_idx);
        }
        else
        {
            return std::make_pair(invalid_idx, invalid_idx);
        }
    }

    TokenView slice(size_t start_idx, size_t length = invalid_idx) const;

    // compares this token span to a string by lexing the string into tokens and comparing each token individually
    bool equals_annotation_string_lexed(std::string_view str) const;

    inline void reset()
    {
        start = nullptr;
        num_tokens = 0;
    }

    std::string to_repr() const;
    std::string to_string() const;

    uint64_t hash() const;

    // Computes the hash of a string as if it was a token. If tok_type is not specified, guesses at the token type
    // based on the value. This is mainly useful for map implementations that use TokenLists as a key.
    static uint64_t hash_string_as_single_token(std::string_view str, TokenType tok_type = TokenType::Invalid);

    // Returns a string view representing the original text of the entire token span.
    // Useful for getting the original whitespace for the full token span.
    FlexString source(bool force_owned = false) const;
};


enum class StringQuoteMode : uint8_t
{
    Auto = 0,
    Double,
    Single,
};


const char* string_quote_mode_to_string(StringQuoteMode mode);


inline std::ostream& operator<<(std::ostream& os, StringQuoteMode type)
{
    return (os << string_quote_mode_to_string(type));
}


struct SerializerSettings
{
    bool pretty_print = true;

    int32_t target_line_length = 80;
    std::string indent = "    ";
    std::string linebreak = "\n";
    std::string key_separator = ": ";
    std::string value_separator = ",\n";
    StringQuoteMode default_quote = StringQuoteMode::Double;
    int32_t default_float_precision = 12;
    bool float_fixed_precision = false;

    static SerializerSettings make_compact()
    {
        SerializerSettings result;
        result.pretty_print = false;
        result.target_line_length = -1;
        result.indent.clear();
        result.linebreak.clear();
        result.key_separator = ":";
        result.value_separator = ",";
        return result;
    }

    inline int32_t get_target_line_length() const
    {
        if (!pretty_print)
        {
            return 0;
        }
        else if (target_line_length > 0)
        {
            return target_line_length;
        }
        else
        {
            return 80;
        }
    }

    std::string to_repr() const;
};


namespace base64
{
    bool is_base64_char(char ch);

    // returns the number of characters that would be needed in a base64 string to store the specified number of bytes
    inline size_t get_base64_string_size(size_t num_bytes)
    {
        return 4 * ((num_bytes + 2) / 3);
    }

    // returns the number of bytes represented in a base64 string
    inline size_t get_num_bytes_in_base64_string(const char* base64_str, size_t base64_str_len)
    {
        if (base64_str_len % 4 != 0)
        {
            // Input data size is not a multiple of 4
            return 0;
        }

        size_t num_bytes = base64_str_len / 4 * 3;

        if (base64_str[base64_str_len - 1] == '=')
        {
            num_bytes--;
        }

        if (base64_str[base64_str_len - 2] == '=')
        {
            num_bytes--;
        }

        return num_bytes;
    }

    // returns the number of bytes represented in a base64 string
    size_t get_num_bytes_in_base64_multiline_string(const char* base64_str, size_t base64_str_len);

    void bytes_to_base64(const uint8_t* bytes, size_t bytes_len, char* out_data, size_t out_size);
    void base64_to_bytes(const char* base64_str, size_t base64_str_len, uint8_t* out_data, size_t out_size);
    size_t base64_multiline_to_bytes(const char* base64_str, size_t base64_str_len, uint8_t* out_data, size_t out_size);

} // namespace base64


namespace detail
{

static constexpr uint8_t hex_char_to_byte_table[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
    0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static_assert(sizeof(hex_char_to_byte_table) == 256, "Expected hex_char_to_byte_table to have exactly 256 entries");

inline uint8_t hex_to_byte(char a, char b)
{
    return (hex_char_to_byte_table[static_cast<size_t>(a)] << 4) | hex_char_to_byte_table[static_cast<size_t>(b)];
}

static constexpr char byte_to_hex_table[] = "0123456789abcdef";

inline void byte_to_hex(uint8_t value, char& out_char_a, char& out_char_b)
{
    out_char_a = byte_to_hex_table[value >> 4];
    out_char_b = byte_to_hex_table[value & 0xF];
}

} // namespace detail


struct TokenList
{
    friend struct TokenView;

    detail::ArrayBuffer<Token, 16> tokens;
    FlexString src;

private:
    template<typename T>
    inline void copy_from_internal(const T& rhs)
    {
        tokens.clear();
        for (size_t i = 0; i < rhs.size(); i++)
        {
            tokens.push(rhs[i].copy());
        }
        src = rhs.source(true);
    }

public:
    TokenList() = default;

    explicit TokenList(TokenView span) { copy_from_internal(span); }

    TokenList(const TokenList& rhs) { copy_from_internal(rhs); }
    TokenList& operator=(const TokenList& rhs) { copy_from_internal(rhs); return *this; }

    TokenList(TokenList&& rhs) = default;
    TokenList& operator=(TokenList&& rhs) = default;

    static std::optional<TokenList> parse(std::string_view source, std::string* out_error = nullptr);
    static std::optional<TokenList> parse_annotation(std::string_view annotation, std::string* out_error = nullptr);
    static std::optional<TokenList> parse_expression(std::string_view expression, std::string* out_error = nullptr);

    static inline TokenList parse_annotation_checked(std::string_view annotation)
    {
        std::string err;
        auto result = parse_annotation(annotation, &err);
        JXC_ASSERTF(result.has_value(), "Failed parsing annotation: {}", err);
        return *result;
    }

    // Assumes that the input string is a valid identifier and returns it as an annotation
    static inline TokenList from_identifier(std::string_view identifier)
    {
        TokenList result;
        JXC_ASSERT(is_valid_identifier(identifier));
        result.tokens.push(Token(TokenType::Identifier, 0, identifier.size() - 1, FlexString::make_owned(identifier)));
        return result;
    }

    void serialize(Serializer& doc) const;

    inline explicit operator bool() const { return tokens.size() > 0; }
    inline Token& operator[](size_t idx) { return tokens[idx]; }
    inline const Token& operator[](size_t idx) const { return tokens[idx]; }

    inline bool operator==(const TokenView& rhs) const { return TokenView(*this) == rhs; }
    inline bool operator!=(const TokenView& rhs) const { return TokenView(*this) != rhs; }

    inline bool operator==(const TokenList& rhs) const { return TokenView(*this) == TokenView(rhs); }
    inline bool operator!=(const TokenList& rhs) const { return TokenView(*this) != TokenView(rhs); }

    inline bool operator==(std::string_view rhs) const { return TokenView(*this) == rhs; }
    inline bool operator!=(std::string_view rhs) const { return TokenView(*this) != rhs; }

    friend bool operator==(std::string_view lhs, const TokenList& rhs) { return rhs.operator==(lhs); }
    friend bool operator!=(std::string_view lhs, const TokenList& rhs) { return rhs.operator!=(lhs); }

    void reset() { tokens.clear(); src.reset(); }

    size_t size() const { return tokens.size(); }

    TokenList slice_copy(size_t start_idx, size_t length) const;

    inline TokenView slice_view(size_t start_idx, size_t length) const { return TokenView(*this).slice(start_idx, length); }

    FlexString source(bool force_owned = false) const;

    inline uint64_t hash() const { return TokenView(*this).hash(); }

    std::string to_repr() const;
    inline std::string to_string() const { return TokenView(*this).to_string(); }
};


JXC_BEGIN_NAMESPACE(detail)

template<typename T>
struct DebugTypeName
{
    static constexpr std::string name() { return typeid(T).name(); }
};

// helper for adding a DebugTypeName specialization
#define JXC_DEFINE_DEBUG_TYPE_NAME(CPP_TYPE) \
    template<> struct ::jxc::DebugTypeName { \
        static constexpr std::string name() { return #CPP_TYPE; } \
    }

template<typename T>
constexpr std::string get_type_name()
{
    // strip pointer/reference/const/volatile
    if constexpr (std::is_pointer_v<T>) { return get_type_name<std::remove_pointer_t<T>>(); }
    else if constexpr (std::is_reference_v<T>) { return get_type_name<std::remove_reference_t<T>>(); }
    else if constexpr (std::is_const_v<T>) { return get_type_name<std::remove_const_t<T>>(); }
    else if constexpr (std::is_volatile_v<T>) { return get_type_name<std::remove_volatile_t<T>>(); }

#define JXC_TYPE_EQUALS(TYPE) constexpr (std::is_same_v<T, TYPE>) { return #TYPE; }

    // C++ language types
    else if JXC_TYPE_EQUALS(const char*)
    else if JXC_TYPE_EQUALS(bool)
    else if JXC_TYPE_EQUALS(uint8_t)
    else if JXC_TYPE_EQUALS(uint16_t)
    else if JXC_TYPE_EQUALS(uint32_t)
    else if JXC_TYPE_EQUALS(uint64_t)
    else if JXC_TYPE_EQUALS(size_t)
    else if JXC_TYPE_EQUALS(int8_t)
    else if JXC_TYPE_EQUALS(int16_t)
    else if JXC_TYPE_EQUALS(int32_t)
    else if JXC_TYPE_EQUALS(int64_t)
    else if JXC_TYPE_EQUALS(char)
    else if JXC_TYPE_EQUALS(wchar_t)
#if JXC_CPP20
    else if JXC_TYPE_EQUALS(char8_t)
#endif
    else if JXC_TYPE_EQUALS(char16_t)
    else if JXC_TYPE_EQUALS(char32_t)
    else if JXC_TYPE_EQUALS(std::string)
    else if JXC_TYPE_EQUALS(std::string_view)
    else if JXC_TYPE_EQUALS(std::wstring)
    else if JXC_TYPE_EQUALS(std::wstring_view)
    else if JXC_TYPE_EQUALS(nullptr_t)

    // JXC types
    else if JXC_TYPE_EQUALS(TokenType)
    else if JXC_TYPE_EQUALS(Token)
    else if JXC_TYPE_EQUALS(TokenView)
    else if JXC_TYPE_EQUALS(TokenList)
    else if JXC_TYPE_EQUALS(FlexString)
    else if JXC_TYPE_EQUALS(BytesView)
    else if JXC_TYPE_EQUALS(SmallByteArray)
    else if JXC_TYPE_EQUALS(FloatLiteralType)
    else if JXC_TYPE_EQUALS(Timer)
    else if JXC_TYPE_EQUALS(Date)
    else if JXC_TYPE_EQUALS(DateTime)
    else if JXC_TYPE_EQUALS(StringQuoteMode)
    else if JXC_TYPE_EQUALS(SerializerSettings)

#undef JXC_TYPE_EQUALS

    // for any other types, use the DebugTypeName trait struct
    return DebugTypeName<T>::name();
}

template<typename T>
struct DebugTypeName<std::vector<T>>
{
    static constexpr const char* name() { return jxc::format("std::vector<{}>", get_type_name<T>()); }
};

template<typename T, size_t MaxSize>
struct DebugTypeName<std::array<T, MaxSize>>
{
    static constexpr const char* name() { return jxc::format("std::array<{}, {}>", get_type_name<T>(), MaxSize); }
};

template<typename T, uint16_t MaxSize>
struct DebugTypeName<FixedArray<T, MaxSize>>
{
    static constexpr const char* name() { return jxc::format("FixedArray<{}, {}>", get_type_name<T>(), MaxSize); }
};

template<typename T, uint16_t MaxSize>
struct DebugTypeName<ArrayBuffer<T, MaxSize>>
{
    static constexpr const char* name() { return jxc::format("ArrayBuffer<{}, {}>", get_type_name<T>(), MaxSize); }
};

template<typename T, uint16_t MaxSize>
struct DebugTypeName<MiniBuffer<T, MaxSize>>
{
    static constexpr const char* name() { return jxc::format("MiniBuffer<{}, {}>", get_type_name<T>(), MaxSize); }
};

JXC_END_NAMESPACE(detail)


JXC_END_NAMESPACE(jxc)
