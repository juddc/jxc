#pragma once
#include "jxc/jxc.h"
#include "jxc/jxc_type_traits.h"
#include <memory>
#include "ankerl/unordered_dense.h"


JXC_BEGIN_NAMESPACE(jxc)

JXC_BEGIN_NAMESPACE(detail)

enum class AnnotationDataType : uint8_t
{
    Empty = 0,
    SourceInline,
    TokenSpanView,
    TokenSpanOwned,
    LAST = TokenSpanOwned,
};


// NB. This struct is intended only for internal use in the Value class - it has a union but no
// type enum field, because that's packed in a bitfield in Value for memory efficiency.
struct AnnotationData
{
    static constexpr size_t max_inline_annotation_source_len = (sizeof(void*) * 2) - 1;
    using token_span_allocator_t = JXC_ALLOCATOR<OwnedTokenSpan>;

    union
    {
        // 7 bytes of inline string storage to avoid allocations for small strings
        struct { char buf[max_inline_annotation_source_len]; uint8_t len; } source_inline;

        // TokenSpan storage
        struct { Token* ptr; size_t len; } tokens_view;

        // owned token storage
        OwnedTokenSpan* tokens_owned;
    };

    AnnotationData()
    {
        clear_internal();
    }

    inline void clear_internal()
    {
        memset((void*)this, 0, sizeof(AnnotationData));
    }

    inline AnnotationDataType clear(AnnotationDataType type)
    {
        if (type != AnnotationDataType::Empty)
        {
            if (type == AnnotationDataType::TokenSpanOwned && tokens_owned != nullptr)
            {
                token_span_allocator_t allocator{};
                std::allocator_traits<token_span_allocator_t>::destroy(allocator, tokens_owned);
                allocator.deallocate(tokens_owned, 1);
            }
            clear_internal();
        }
        return AnnotationDataType::Empty;
    }

    inline bool is_empty(AnnotationDataType type) const
    {
        return type == AnnotationDataType::Empty
            || (type == AnnotationDataType::SourceInline && source_inline.len == 0)
            || (type == AnnotationDataType::TokenSpanView && tokens_view.len == 0)
            || (type == AnnotationDataType::TokenSpanOwned && (tokens_owned == nullptr || tokens_owned->size() == 0));
    }

    inline std::string_view as_source_inline_view_unchecked() const
    {
        return (source_inline.len > 0) ? std::string_view{ &source_inline.buf[0], static_cast<size_t>(source_inline.len) } : std::string_view{};
    }

    inline TokenSpan as_token_span_unchecked() const
    {
        return TokenSpan{ *tokens_view.ptr, tokens_view.len };
    }

    inline OwnedTokenSpan& as_owned_token_span_unchecked() { return *tokens_owned; }
    inline const OwnedTokenSpan& as_owned_token_span_unchecked() const { return *tokens_owned; }
    inline TokenSpan as_owned_token_span_view_unchecked() const { return (tokens_owned != nullptr) ? TokenSpan(*tokens_owned) : TokenSpan(); }

    inline bool have_unowned_data(AnnotationDataType type) const
    {
        return type == AnnotationDataType::TokenSpanView && tokens_view.ptr != nullptr && tokens_view.len > 0;
    }

    inline bool have_owned_data(AnnotationDataType type) const
    {
        return type == AnnotationDataType::TokenSpanOwned && tokens_owned != nullptr;
    }

    AnnotationDataType convert_view_to_owned(AnnotationDataType orig_type)
    {
        if (orig_type != AnnotationDataType::TokenSpanView)
        {
            return orig_type;
        }
        TokenSpan anno_view = as_token_span_unchecked();
        return assign_tokens_owned_copy_from_view(orig_type, anno_view);
    }

    AnnotationDataType copy_from(AnnotationDataType prev_type, AnnotationDataType rhs_type, const AnnotationData& rhs)
    {
        switch (rhs_type)
        {
        case AnnotationDataType::Empty:
            return clear(prev_type);
        case AnnotationDataType::SourceInline:
            return assign_source_inline(prev_type, rhs.as_source_inline_view_unchecked());
        case AnnotationDataType::TokenSpanView:
            return assign_tokens_view(prev_type, rhs.as_token_span_unchecked());
        case AnnotationDataType::TokenSpanOwned:
            return (rhs.tokens_owned != nullptr) ? assign_tokens_owned_copy(prev_type, rhs.as_owned_token_span_unchecked()) : clear(prev_type);
        default:
            break;
        }
        JXC_ASSERTF(false, "Invalid AnnotationDataType {}", static_cast<size_t>(rhs_type));
        return AnnotationDataType::Empty;
    }

    AnnotationDataType move_from(AnnotationDataType prev_type, AnnotationDataType rhs_type, AnnotationData&& rhs)
    {
        switch (rhs_type)
        {
        case AnnotationDataType::Empty:
            return clear(prev_type);
        case AnnotationDataType::SourceInline:
            return assign_source_inline(prev_type, rhs.as_source_inline_view_unchecked());
        case AnnotationDataType::TokenSpanView:
            return assign_tokens_view(prev_type, rhs.as_token_span_unchecked());
        case AnnotationDataType::TokenSpanOwned:
            clear(prev_type);
            tokens_owned = rhs.tokens_owned;
            rhs.clear_internal();
            return rhs_type;
        default:
            break;
        }
        JXC_ASSERTF(false, "Invalid AnnotationDataType {}", static_cast<size_t>(rhs_type));
        return AnnotationDataType::Empty;
    }

    AnnotationDataType assign_source_inline(AnnotationDataType prev_type, std::string_view source_view)
    {
        clear(prev_type);
        JXC_DEBUG_ASSERT(source_view.size() <= max_inline_annotation_source_len);
        JXC_STRNCPY(&source_inline.buf[0], max_inline_annotation_source_len, source_view.data(), source_view.size());
        source_inline.len = static_cast<uint8_t>(source_view.size());
        return AnnotationDataType::SourceInline;
    }

    AnnotationDataType assign_tokens_view(AnnotationDataType prev_type, TokenSpan token_span)
    {
        clear(prev_type);
        tokens_view.ptr = token_span.start;
        tokens_view.len = token_span.num_tokens;
        return AnnotationDataType::TokenSpanView;
    }

    AnnotationDataType assign_tokens_owned_copy_from_view(AnnotationDataType prev_type, TokenSpan token_span)
    {
        clear(prev_type);
        token_span_allocator_t allocator{};
        JXC_DEBUG_ASSERT(token_span.start != nullptr || token_span.num_tokens == 0);
        tokens_owned = allocator.allocate(1);
        memset((void*)tokens_owned, 0, sizeof(OwnedTokenSpan)); // zero out memory first
        std::allocator_traits<decltype(allocator)>::construct(allocator, tokens_owned, token_span);
        return AnnotationDataType::TokenSpanOwned;
    }

    AnnotationDataType assign_tokens_owned_copy(AnnotationDataType prev_type, const OwnedTokenSpan& token_span)
    {
        clear(prev_type);
        token_span_allocator_t allocator{};
        JXC_DEBUG_ASSERT(token_span.size() > 0);
        tokens_owned = allocator.allocate(1);
        memset((void*)tokens_owned, 0, sizeof(OwnedTokenSpan)); // zero out memory first
        std::allocator_traits<decltype(allocator)>::construct(allocator, tokens_owned, token_span);
        return AnnotationDataType::TokenSpanOwned;
    }

    AnnotationDataType assign_tokens_owned_move(AnnotationDataType prev_type, OwnedTokenSpan&& token_span)
    {
        clear(prev_type);
        token_span_allocator_t allocator{};
        JXC_DEBUG_ASSERT(token_span.size() > 0);
        tokens_owned = allocator.allocate(1);
        memset((void*)tokens_owned, 0, sizeof(OwnedTokenSpan)); // zero out memory first
        std::allocator_traits<decltype(allocator)>::construct(allocator, tokens_owned, std::move(token_span));
        return AnnotationDataType::TokenSpanOwned;
    }

    bool eq(AnnotationDataType lhs_type, AnnotationDataType rhs_type, const AnnotationData& rhs) const
    {
        auto token_value_equals_token_span = [](std::string_view tok_value, TokenSpan span) -> bool
        {
            // assume tok_value is a single token
            const size_t num_tokens = span.size();
            if (num_tokens > 1) { return false; }
            if (tok_value.size() == 0) { return num_tokens == 0; }
            return token_type_has_value(span[0].type)
                ? (tok_value == span[0].value.as_view())
                : (tok_value == token_type_to_symbol(span[0].type));
        };

        switch (lhs_type)
        {
        case AnnotationDataType::Empty:
            return rhs.is_empty(rhs_type);

        case AnnotationDataType::SourceInline:
            switch (rhs_type)
            {
            case AnnotationDataType::Empty:
                return source_inline.len == 0;
            case AnnotationDataType::SourceInline:
                return as_source_inline_view_unchecked() == rhs.as_source_inline_view_unchecked();
            case AnnotationDataType::TokenSpanView:
                return token_value_equals_token_span(as_source_inline_view_unchecked(), rhs.as_token_span_unchecked());
            case AnnotationDataType::TokenSpanOwned:
                return token_value_equals_token_span(as_source_inline_view_unchecked(), rhs.as_owned_token_span_view_unchecked());
            }
            break;

        case AnnotationDataType::TokenSpanView:
            switch (rhs_type)
            {
            case AnnotationDataType::Empty:
                return tokens_view.ptr == nullptr || tokens_view.len == 0;
            case AnnotationDataType::SourceInline:
                return token_value_equals_token_span(rhs.as_source_inline_view_unchecked(), as_token_span_unchecked());
            case AnnotationDataType::TokenSpanView:
                return as_token_span_unchecked() == rhs.as_token_span_unchecked();
            case AnnotationDataType::TokenSpanOwned:
                return as_token_span_unchecked() == rhs.as_owned_token_span_view_unchecked();
            }
            break;

        case AnnotationDataType::TokenSpanOwned:
            switch (rhs_type)
            {
            case AnnotationDataType::Empty:
                return tokens_owned == nullptr || tokens_owned->size() == 0;
            case AnnotationDataType::SourceInline:
                return token_value_equals_token_span(rhs.as_source_inline_view_unchecked(), as_owned_token_span_view_unchecked());
            case AnnotationDataType::TokenSpanView:
                return as_owned_token_span_view_unchecked() == rhs.as_token_span_unchecked();
            case AnnotationDataType::TokenSpanOwned:
                return as_owned_token_span_view_unchecked() == rhs.as_owned_token_span_view_unchecked();
            }
            break;
        }

        JXC_UNREACHABLE("Invalid AnnotationDataType {}", static_cast<size_t>(lhs_type));
        return false;
    }

    TokenSpan as_token_span(AnnotationDataType type) const
    {
        switch (type)
        {
        case AnnotationDataType::Empty:
            // fallthrough
        case AnnotationDataType::SourceInline:
            return TokenSpan{};
        case AnnotationDataType::TokenSpanView:
            return as_token_span_unchecked();
        case AnnotationDataType::TokenSpanOwned:
            return (tokens_owned != nullptr) ? TokenSpan(*tokens_owned) : TokenSpan{};
        default:
            break;
        }
        JXC_UNREACHABLE("Invalid AnnotationDataType {}", static_cast<size_t>(type));
        return TokenSpan{};
    }

    FlexString as_source(AnnotationDataType type) const
    {
        static_assert(FlexString::max_inline_len >= max_inline_annotation_source_len,
            "Expected FlexString inline buffer to fit the inline annotation source string");
        switch (type)
        {
        case AnnotationDataType::Empty:
            break;
        case AnnotationDataType::SourceInline:
            return FlexString::make_inline(as_source_inline_view_unchecked());
        case AnnotationDataType::TokenSpanView:
            return as_token_span_unchecked().source();
        case AnnotationDataType::TokenSpanOwned:
            if (tokens_owned != nullptr)
            {
                return tokens_owned->src;
            }
            break;
        default:
            break;
        }
        JXC_UNREACHABLE("Invalid AnnotationDataType {}", static_cast<size_t>(type));
        return FlexString();
    }
};

static_assert(sizeof(AnnotationData) == sizeof(void*) * 2, "Expected AnnotationData to be the same size as two pointers");


template<traits::Number T, traits::Number ValT>
constexpr bool value_fits_in_type(ValT value)
{
    if constexpr (std::same_as<T, ValT>)
    {
        return true;
    }
    else if constexpr (std::integral<T> && std::integral<ValT>)
    {
        // two int types means we can use std::cmp_less_equal and std::cmp_greater_equal
        if constexpr (std::unsigned_integral<T>)
        {
            return value >= static_cast<ValT>(0) && std::cmp_less_equal(value, std::numeric_limits<T>::max());
        }
        else
        {
            return std::cmp_greater_equal(value, std::numeric_limits<T>::min()) && std::cmp_less_equal(value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::floating_point<T> && std::integral<ValT>)
    {
        // does the int value fit inside floating point type T?
        return std::cmp_greater_equal(value, traits::detail::min_int_that_fits_in_float<T>()) && std::cmp_less_equal(value, traits::detail::max_int_that_fits_in_float<T>());
    }
    else if constexpr (std::integral<T> && std::floating_point<ValT>)
    {
        // does any float value of type ValT fit inside the int type T?
        constexpr bool safe_to_cast = std::cmp_greater_equal(traits::detail::min_int_that_fits_in_float<ValT>(), std::numeric_limits<T>::min())
            && std::cmp_less_equal(traits::detail::max_int_that_fits_in_float<ValT>(), std::numeric_limits<T>::max());
        if constexpr (safe_to_cast)
        {
            return true;
        }
        else
        {
            return value_fits_in_type<T>(static_cast<int64_t>(std::round(value)));
        }
    }
    else if constexpr (std::floating_point<T> && std::floating_point<ValT>)
    {
        return static_cast<double>(value) >= static_cast<double>(std::numeric_limits<T>::min())
            && static_cast<double>(value) <= static_cast<double>(std::numeric_limits<T>::max());
    }

    JXC_UNREACHABLE("Unreachable");
    return false;
}


// NB. This struct is intended only for internal use in the Value class.
// It's used inside a union and should be trivially constructible and trivially copyable
template<typename T, size_t MaxTagLen>
struct TaggedNum
{
    static constexpr size_t max_number_tag_len = MaxTagLen;

    T value;
    char tag[max_number_tag_len];
    uint8_t tag_len;

    inline void set_tag(std::string_view new_tag)
    {
        JXC_ASSERTF(new_tag.size() <= max_number_tag_len,
            "Number suffix size {} must be <= {}", new_tag.size(), max_number_tag_len);
        if (new_tag.data() != nullptr && new_tag.size() > 0)
        {
            JXC_STRNCPY(&tag[0], max_number_tag_len, new_tag.data(), new_tag.size());
            tag_len = static_cast<uint8_t>(new_tag.size());
        }
        else
        {
            clear_tag();
        }
    }

    inline std::string_view get_tag() const { return std::string_view{ &tag[0], static_cast<size_t>(tag_len) }; }
    inline void clear_tag() { memset(&tag[0], '\0', max_number_tag_len); tag_len = 0; }
    inline void set(T new_value) { value = new_value; clear_tag(); }
    inline void set(T new_value, std::string_view new_tag) { value = new_value; set_tag(new_tag); }

    inline bool eq(const TaggedNum& rhs) const
    {
        if constexpr (std::floating_point<T>)
        {
            return std::abs(value - rhs.value) < std::numeric_limits<T>::epsilon() && get_tag() == rhs.get_tag();
        }
        else
        {
            return value == rhs.value && get_tag() == rhs.get_tag();
        }
    }

    template<typename RhsT, size_t RhsMaxTagLen>
        requires (!std::same_as<T, RhsT>)
    inline bool eq(const TaggedNum<RhsT, RhsMaxTagLen>& rhs) const
    {
        if constexpr (std::integral<T> && std::integral<RhsT>)
        {
            return value_fits_in_type<T>(rhs.value)
                && value == static_cast<T>(rhs.value)
                && get_tag() == rhs.get_tag();
        }
        else if constexpr (std::floating_point<T> && std::floating_point<RhsT>)
        {
            return value_fits_in_type<T>(rhs.value)
                && std::abs(static_cast<double>(value) - static_cast<double>(rhs.value)) < std::numeric_limits<double>::epsilon()
                && get_tag() == rhs.get_tag();
        }
        else if constexpr (std::integral<T> && std::floating_point<RhsT>)
        {
            return value_fits_in_type<T>(rhs.value)
                && (double)static_cast<int64_t>(rhs.value) == rhs.value // rhs float value is an integer
                && value == static_cast<T>(rhs.value)
                && get_tag() == rhs.get_tag();
        }
        else if constexpr (std::floating_point<T> && std::integral<RhsT>)
        {
            return value_fits_in_type<RhsT>(value)
                && (double)static_cast<int64_t>(value) == value // our float value is an integer
                && static_cast<RhsT>(value) == rhs.value
                && get_tag() == rhs.get_tag();
        }
        else
        {
            JXC_UNREACHABLE("Unreachable");
            return false;
        }
    }

    size_t to_string_in_buffer(char* buffer, size_t buffer_len, int float_precision, bool fixed_precision, bool include_tag) const
    {
        JXC_DEBUG_ASSERT(buffer != nullptr);
        JXC_DEBUG_ASSERT(buffer_len >= 32);

        std::string number_str;

        if constexpr (traits::Float<T>)
        {
            if (float_precision <= 0)
            {
                number_str = jxc::format("{}", static_cast<int64_t>(std::round(value)));
            }
            else
            {
                number_str = jxc::format("{}", FloatPrecision(value, float_precision));
                if (!fixed_precision)
                {
                    // remove trailing zeros as long as there are multiple zeroes in a row
                    std::string_view view = number_str;
                    while (view.size() >= 2 && view[view.size() - 2] != '.' && view[view.size() - 1] == '0')
                    {
                        view = view.substr(0, view.size() - 1);
                    }
                    number_str = std::string(view);
                }
            }
        }
        else
        {
            number_str = jxc::format("{}", value);
        }

        JXC_ASSERT(number_str.size() <= buffer_len);
        JXC_STRNCPY(buffer, buffer_len, number_str.data(), number_str.size());

        if (include_tag && tag_len > 0)
        {
            const size_t buffer_num_str_len = number_str.size();
            const size_t tag_size = static_cast<size_t>(tag_len);
            JXC_ASSERT(buffer_num_str_len + tag_size <= buffer_len);
            for (size_t i = 0; i < tag_size; i++)
            {
                buffer[buffer_num_str_len + i] = tag[i];
            }
            return buffer_num_str_len + tag_size;
        }

        return number_str.size();
    }

    std::string to_string(int float_precision, bool fixed_precision, bool include_tag) const
    {
        static constexpr size_t buf_len = 64;
        char buf[buf_len];
        const size_t result_len = to_string_in_buffer(buf, buf_len, float_precision, fixed_precision, include_tag);
        return std::string(buf, result_len);
    }
};

static_assert(std::is_trivially_constructible_v<TaggedNum<int64_t, 15>>, "TaggedNum should be trivially constructible");
static_assert(std::is_trivially_copyable_v<TaggedNum<int64_t, 15>>, "TaggedNum should be trivially copyable");

JXC_END_NAMESPACE(detail)

class Value;

JXC_END_NAMESPACE(jxc)

// std::hash overload for jxc::Value (forward declaration)
template<>
struct std::hash<jxc::Value>
{
    inline size_t operator()(const jxc::Value& value) const;
};

JXC_BEGIN_NAMESPACE(jxc)

enum class ValueType : uint8_t
{
    Invalid = 0,
    Null,
    Bool,
    SignedInteger,
    UnsignedInteger,
    Float,
    String,
    Bytes,
    Array,
    Object,
    COUNT,
};


const char* value_type_to_string(ValueType type);


inline std::ostream& operator<<(std::ostream& os, ValueType type)
{
    return (os << value_type_to_string(type));
}


class Value
{
public:
    // variant value types
    using bool_vt = bool;
    using signed_integer_vt = int64_t;
    using unsigned_integer_vt = uint64_t;
    using float_vt = double;
    using string_view_vt = std::string_view;
    using bytes_view_vt = BytesView;

    // odd number so we can stick an extra uint8_t at the end for length
    static constexpr size_t byte_buffer_len = 23;
    using size_type = size_t;

    using byte_allocator_t = JXC_ALLOCATOR<uint8_t>;
    using value_allocator_t = JXC_ALLOCATOR<Value>;
    using pair_allocator_t = JXC_ALLOCATOR<std::pair<Value, Value>>;

    using array_vt = std::vector<Value, value_allocator_t>;

    struct object_hash_t { auto operator()(const Value& val) const -> uint64_t { return val.hash(); } };
    using object_vt = ankerl::unordered_dense::map<Value, Value, object_hash_t, std::equal_to<Value>, pair_allocator_t>;

    using array_allocator_t = JXC_ALLOCATOR<array_vt>;
    using object_allocator_t = JXC_ALLOCATOR<object_vt>;

    // tags for explicit string/bytes storage mechanism
    struct AsView {};
    struct AsInline {};
    struct AsOwned {};

    static constexpr size_t max_inline_string_len = byte_buffer_len;
    static constexpr size_t max_inline_bytes_len = byte_buffer_len;
    static constexpr size_t max_number_tag_len = byte_buffer_len - sizeof(uint64_t);

    static_assert(max_number_tag_len >= 15, "Number tags must have a buffer that can fit at least 15 chars");

    using tagged_signed_integer_vt = detail::TaggedNum<signed_integer_vt, max_number_tag_len>;
    using tagged_unsigned_integer_vt = detail::TaggedNum<unsigned_integer_vt, max_number_tag_len>;
    using tagged_float_vt = detail::TaggedNum<float_vt, max_number_tag_len>;

private:
    static uint8_t* create_buffer(size_t buf_len);
    static void destroy_buffer(uint8_t* buf, size_t buf_len);

private:
    using AnnotationDataType = detail::AnnotationDataType;

    enum class ByteBufferType : uint8_t
    {
        Inline = 0,
        View,
        Owned,
        LAST = Owned,
    };

    static_assert(static_cast<size_t>(ValueType::COUNT) - 1 <= 0b1111, "ValueType needs to fit in 4 bits");

    // Ideally we could just use COUNT members for these two for consistency, but then GCC complains that all enum values don't fit in the bitfield.
    static_assert(static_cast<size_t>(AnnotationDataType::LAST) <= 0b11, "AnnotationDataType needs to fit in 2 bits");
    static_assert(static_cast<size_t>(ByteBufferType::LAST) <= 0b11, "ByteBufferType needs to fit in 2 bits.");

    // 8 bits of storage for value type and related metadata
    struct Metadata
    {
        ValueType data : 4;
        ByteBufferType buffer : 2;
        AnnotationDataType anno : 2;

        static inline Metadata init(
            ValueType type_data = ValueType::Invalid,
            ByteBufferType type_buffer = ByteBufferType::Inline,
            AnnotationDataType type_anno = AnnotationDataType::SourceInline)
        {
            Metadata result;
            result.data = type_data;
            result.buffer = type_buffer;
            result.anno = type_anno;
            return result;
        }

        inline void reset(ValueType type_data = ValueType::Invalid, ByteBufferType type_buffer = ByteBufferType::Inline)
        {
            data = type_data;
            buffer = type_buffer;
        }
    } type;

    static_assert(sizeof(Metadata) == sizeof(uint8_t), "Metadata struct should be 8 bits");

    detail::AnnotationData annotation;

    // variant value storage
    union DataStore
    {
        bool_vt value_bool;
        tagged_signed_integer_vt value_signed_integer;
        tagged_unsigned_integer_vt value_unsigned_integer;
        tagged_float_vt value_float;
        struct { uint8_t* ptr; size_t len; } buffer_ptr; // string view, bytes view, owned string ptr, or owned bytes ptr
        struct { uint8_t bytes[byte_buffer_len]; uint8_t len; } buffer_inline; // inline string or inline bytes
        array_vt* value_array;
        object_vt* value_object;

        DataStore() { clear(); }

        inline void clear() { memset(this, 0, sizeof(DataStore)); }

        inline string_view_vt get_buffer_ptr_as_string_view() const { return string_view_vt{ reinterpret_cast<const char*>(buffer_ptr.ptr), buffer_ptr.len }; }
        inline bytes_view_vt get_buffer_ptr_as_bytes_view() const { return bytes_view_vt{ buffer_ptr.ptr, buffer_ptr.len }; }
        inline void clear_buffer_ptr() { buffer_ptr.ptr = nullptr; buffer_ptr.len = 0; }
        inline void set_buffer_ptr(string_view_vt val) { buffer_ptr.ptr = (uint8_t*)const_cast<char*>(val.data()); buffer_ptr.len = val.size(); }
        inline void set_buffer_ptr(bytes_view_vt val) { buffer_ptr.ptr = const_cast<uint8_t*>(val.data()); buffer_ptr.len = val.size(); }

        inline string_view_vt get_buffer_inline_as_string_view() const { return string_view_vt{ reinterpret_cast<const char*>(&buffer_inline.bytes[0]), static_cast<size_t>(buffer_inline.len) }; }
        inline bytes_view_vt get_buffer_inline_as_bytes_view() const { return bytes_view_vt{ &buffer_inline.bytes[0], static_cast<size_t>(buffer_inline.len) }; }
        inline void clear_buffer_inline() { memset(&buffer_inline.bytes[0], 0, byte_buffer_len); buffer_inline.len = 0; }
        inline void set_buffer_inline(string_view_vt val)
        {
            JXC_DEBUG_ASSERT(val.data() != nullptr);
            JXC_DEBUG_ASSERT(val.size() <= byte_buffer_len);
            JXC_STRNCPY(reinterpret_cast<char*>(&buffer_inline.bytes[0]), byte_buffer_len, reinterpret_cast<const char*>(val.data()), val.size());
            buffer_inline.len = static_cast<uint8_t>(val.size());
        }
        inline void set_buffer_inline(bytes_view_vt val)
        {
            JXC_DEBUG_ASSERT(val.data() != nullptr);
            JXC_DEBUG_ASSERT(val.size() <= byte_buffer_len);
            JXC_MEMCPY(&buffer_inline.bytes[0], byte_buffer_len, val.data(), val.size());
            buffer_inline.len = static_cast<uint8_t>(val.size());
        }
    } data;

    void alloc_buffer_from(string_view_vt val);
    void alloc_buffer_from(bytes_view_vt val);
    void free_buffer();

    array_vt& alloc_array();
    void free_array();

    object_vt& alloc_object();
    void free_object();

    void copy_from_internal(const Value& rhs);
    void move_from_internal(Value&& rhs);

    // Sets the value to a string, auto-selecting inline or owned depending on length.
    // Intended to be used from assignment operators.
    void assign_from_view(string_view_vt view);

    // Sets the value to a byte array, auto-selecting inline or owned depending on length.
    // Intended to be used from assignment operators.
    void assign_from_view(bytes_view_vt view);

    template<typename T>
        requires traits::Array<T, Value> || std::same_as<T, std::initializer_list<Value>>
    void assign_from_container(const T & value)
    {
        type.reset(ValueType::Array);
        data.value_array = nullptr;
        if constexpr (!std::same_as<T, tag::Array>)
        {
            const size_t num_items = value.size();
            if (num_items > 0)
            {
                array_vt& arr = alloc_array();
                if constexpr (std::same_as<T, array_vt>)
                {
                    arr = value;
                }
                else if constexpr (std::same_as<T, std::initializer_list<Value>>)
                {
                    arr.reserve(num_items);
                    for (const auto& val : value)
                    {
                        arr.push_back(val);
                    }
                }
                else
                {
                    arr.reserve(num_items);
                    for (size_t i = 0; i < num_items; i++)
                    {
                        arr.push_back(value[i]);
                    }
                }
            }
        }
    }

    template<traits::Object<Value, Value> T>
    void assign_from_container(const T& value)
    {
        type.reset(ValueType::Object);
        data.value_object = nullptr;
        if constexpr (!std::same_as<T, tag::Object>)
        {
            const size_t num_items = value.size();
            if (num_items > 0)
            {
                object_vt& obj = alloc_object();
                if constexpr (std::same_as<T, object_vt>)
                {
                    obj = value;
                }
                else
                {
                    obj.reserve(num_items);
                    for (const auto& pair : value)
                    {
                        obj.insert_or_assign(pair.first, pair.second);
                    }
                }
            }
        }
    }

    void assign_from_container(std::initializer_list<std::pair<Value, Value>> value)
    {
        type.reset(ValueType::Object);
        const size_t num_items = value.size();
        if (num_items > 0)
        {
            object_vt& obj = alloc_object();
            obj.reserve(num_items);
            for (const auto& pair : value)
            {
                obj.insert_or_assign(pair.first, pair.second);
            }
        }
        else
        {
            data.value_object = nullptr;
        }
    }

    inline size_t bytes_or_string_len_unchecked() const
    {
        return (type.buffer == ByteBufferType::Inline) ? static_cast<size_t>(data.buffer_inline.len) : data.buffer_ptr.len;
    }

    inline string_view_vt as_string_view_unchecked() const
    {
        return (type.buffer == ByteBufferType::Inline) ? data.get_buffer_inline_as_string_view() : data.get_buffer_ptr_as_string_view();
    }

    inline bytes_view_vt as_bytes_view_unchecked() const
    {
        return (type.buffer == ByteBufferType::Inline) ? data.get_buffer_inline_as_bytes_view() : data.get_buffer_ptr_as_bytes_view();
    }

    inline array_vt& as_array_unchecked() { return *data.value_array; }
    inline const array_vt& as_array_unchecked() const { return *data.value_array; }
    inline array_vt& as_array_unchecked_auto_init() { return (data.value_array != nullptr) ? *data.value_array : alloc_array(); }

    inline object_vt& as_object_unchecked() { return *data.value_object; }
    inline const object_vt& as_object_unchecked() const { return *data.value_object; }
    inline object_vt& as_object_unchecked_auto_init() { return (data.value_object != nullptr) ? *data.value_object : alloc_object(); }

    // to_repr() helpers
    static std::string value_to_string_internal(const Value& val, bool repr_mode, int float_precision, bool fixed_precision);
    static std::string array_to_string_internal(const Value::array_vt& val, bool repr_mode, int float_precision, bool fixed_precision);
    static std::string object_to_string_internal(const Value::object_vt& val, bool repr_mode, int float_precision, bool fixed_precision);

public:
    Value() : type(Metadata::init(ValueType::Invalid)) {}
    Value(ValueType in_type);

    template<traits::Invalid T>
    Value(T) : type(Metadata::init(ValueType::Invalid)) { }

    template<traits::Invalid T>
    Value& operator=(T)
    {
        // reset() sets type to Invalid anyway, so this is all we need
        reset();
        return *this;
    }

    template<traits::Null T>
    Value(T) : type(Metadata::init(ValueType::Null)) {}

    template<traits::Null T>
    Value& operator=(T)
    {
        reset();
        type.reset(ValueType::Null);
        return *this;
    }

    template<traits::Bool T>
    Value(T value)
        : type(Metadata::init(ValueType::Bool))
    {
        data.value_bool = traits::cast_bool<bool_vt>(value);
    }

    template<traits::Bool T>
    Value& operator=(T value)
    {
        reset();
        type.reset(ValueType::Bool);
        data.value_bool = traits::cast_bool<bool_vt>(value);
        return *this;
    }

    Value(tag::Integer)
        : type(Metadata::init(ValueType::SignedInteger))
    {
        data.value_signed_integer.set(static_cast<signed_integer_vt>(0));
    }

    template<traits::SignedInteger T>
    Value(T value)
        : type(Metadata::init(ValueType::SignedInteger))
    {
        data.value_signed_integer.set(traits::cast_integer_clamped<signed_integer_vt>(value));
    }

    template<traits::SignedInteger T>
    Value(T value, string_view_vt tag)
        : type(Metadata::init(ValueType::SignedInteger))
    {
        data.value_signed_integer.set(traits::cast_integer_clamped<signed_integer_vt>(value), tag);
    }

    template<traits::SignedInteger T>
    Value& operator=(T value)
    {
        reset();
        type.reset(ValueType::SignedInteger);
        data.value_signed_integer.set(traits::cast_integer_clamped<signed_integer_vt>(value));
        return *this;
    }

    template<traits::UnsignedInteger T>
    Value(T value)
        : type(Metadata::init(ValueType::UnsignedInteger))
    {
        data.value_unsigned_integer.set(traits::cast_integer_clamped<unsigned_integer_vt>(value));
    }

    template<traits::UnsignedInteger T>
    Value(T value, string_view_vt tag)
        : type(Metadata::init(ValueType::UnsignedInteger))
    {
        data.value_unsigned_integer.set(traits::cast_integer_clamped<unsigned_integer_vt>(value), tag);
    }

    template<traits::UnsignedInteger T>
    Value& operator=(T value)
    {
        reset();
        type.reset(ValueType::UnsignedInteger);
        data.value_unsigned_integer.set(traits::cast_integer_clamped<unsigned_integer_vt>(value));
        return *this;
    }

    template<traits::Float T>
    Value(T value)
        : type(Metadata::init(ValueType::Float))
    {
        data.value_float.set(traits::cast_float_clamped<float_vt>(value));
    }

    template<traits::Float T>
    Value(T value, string_view_vt tag)
        : type(Metadata::init(ValueType::Float))
    {
        data.value_float.set(traits::cast_float_clamped<float_vt>(value), tag);
    }

    template<traits::Float T>
    Value& operator=(T value)
    {
        reset();
        type.reset(ValueType::Float);
        data.value_float.set(traits::cast_float_clamped<float_vt>(value));
        return *this;
    }

    template<traits::StringContainer T>
    Value(const T& value)
    {
        auto view = traits::cast_string_to_view(value);
        if (view.size() <= max_inline_string_len)
        {
            type = Metadata::init(ValueType::String, ByteBufferType::Inline);
            if (view.data() != nullptr)
            {
                data.set_buffer_inline(view);
            }
            else
            {
                data.clear_buffer_inline();
            }
        }
        else
        {
            type = Metadata::init(ValueType::String, ByteBufferType::Owned);
            alloc_buffer_from(view);
        }
    }

    template<traits::StringContainer T>
    Value(const T& value, AsInline)
        : type(Metadata::init(ValueType::String, ByteBufferType::Inline))
    {
        auto view = traits::cast_string_to_view(value);
        JXC_ASSERT(view.size() <= max_inline_string_len);
        data.set_buffer_inline(view);
    }

    template<traits::StringContainer T>
    Value(const T& value, AsView)
        : type(Metadata::init(ValueType::String, ByteBufferType::View))
    {
        data.set_buffer_ptr(traits::cast_string_to_view(value));
    }

    template<traits::StringContainer T>
    Value(const T& value, AsOwned)
    {
        auto view = traits::cast_string_to_view(value);
        if (view.size() > 0)
        {
            type = Metadata::init(ValueType::String, ByteBufferType::Owned);
            alloc_buffer_from(view);
        }
        else
        {
            type = Metadata::init(ValueType::String, ByteBufferType::Inline);
            data.clear_buffer_inline();
        }
    }

    template<traits::StringContainer T>
    Value& operator=(const T& value)
    {
        assign_from_view(traits::cast_string_to_view(value));
        return *this;
    }

    // overloads for string literals
    Value(const char* value) : Value(string_view_vt(value)) {}
    Value(const char* value, size_t value_len) : Value(string_view_vt(value, value_len)) {}

    Value(const char* value, AsInline tag) : Value(string_view_vt(value), tag) {}
    Value(const char* value, size_t value_len, AsInline tag) : Value(string_view_vt(value, value_len), tag) {}

    Value(const char* value, AsView tag) : Value(string_view_vt(value), tag) {}
    Value(const char* value, size_t value_len, AsView tag) : Value(string_view_vt(value, value_len), tag) {}

    Value(const char* value, AsOwned tag) : Value(string_view_vt(value), tag) {}
    Value(const char* value, size_t value_len, AsOwned tag) : Value(string_view_vt(value, value_len), tag) {}

    Value& operator=(const char* value) { assign_from_view(traits::cast_string_to_view(value)); return *this; }

    template<traits::Bytes T>
    Value(const T& value)
    {
        auto view = traits::cast_bytes_to_view(value);
        if (view.size() <= max_inline_bytes_len)
        {
            type = Metadata::init(ValueType::Bytes, ByteBufferType::Inline);
            if (view.data() != nullptr)
            {
                data.set_buffer_inline(view);
            }
            else
            {
                data.clear_buffer_inline();
            }
        }
        else
        {
            type = Metadata::init(ValueType::Bytes, ByteBufferType::Owned);
            alloc_buffer_from(view);
        }
    }

    template<traits::Bytes T>
    Value(const T& value, AsInline)
        : type(Metadata::init(ValueType::Bytes, ByteBufferType::Inline))
    {
        auto view = traits::cast_bytes_to_view(value);
        JXC_ASSERT(view.size() <= max_inline_bytes_len);
        data.set_buffer_inline(view);
    }

    template<traits::Bytes T>
    Value(const T& value, AsView)
        : type(Metadata::init(ValueType::Bytes, ByteBufferType::View))
    {
        data.set_buffer_ptr(traits::cast_bytes_to_view(value));
    }

    template<traits::Bytes T>
    Value(const T& value, AsOwned)
    {
        auto view = traits::cast_bytes_to_view(value);
        if (view.size() > 0)
        {
            type = Metadata::init(ValueType::Bytes, ByteBufferType::Owned);
            alloc_buffer_from(view);
        }
        else
        {
            type = Metadata::init(ValueType::Bytes, ByteBufferType::Inline);
            data.clear_buffer_inline();
        }
    }

    template<traits::Bytes T>
    Value& operator=(const T& value)
    {
        reset();
        auto view = traits::cast_bytes_to_view(value);
        if (view.size() <= max_inline_bytes_len)
        {
            type.reset(ValueType::Bytes, ByteBufferType::Inline);
            data.set_buffer_inline(view);
        }
        else
        {
            type.reset(ValueType::Bytes, ByteBufferType::Owned);
            alloc_buffer_from(view);
        }
        return *this;
    }

    template<traits::Array<Value> T>
    Value(const T& value)
        : type(Metadata::init(ValueType::Array))
    {
        assign_from_container(value);
    }

    template<traits::Array<Value> T>
    Value& operator=(const T& value)
    {
        reset();
        type.reset(ValueType::Array);
        assign_from_container(value);
        return *this;
    }

    Value(std::initializer_list<Value> values)
        : type(Metadata::init(ValueType::Array))
    {
        assign_from_container(values);
    }

    Value& operator=(std::initializer_list<Value> values)
    {
        reset();
        type.reset(ValueType::Array);
        assign_from_container(values);
        return *this;
    }

    template<traits::Object<Value, Value> T>
    Value(const T& value)
        : type(Metadata::init(ValueType::Object))
    {
        assign_from_container(value);
    }

    template<traits::Object<Value, Value> T>
    Value& operator=(const T& value)
    {
        reset();
        type.reset(ValueType::Object);
        assign_from_container(value);
        return *this;
    }

    // TODO: figure out how to disambiguate this from the std::initializer_list array constructor
    //Value(std::initializer_list<std::pair<Value, Value>> values)
    //    : type(Metadata::init(ValueType::Object))
    //{
    //    assign_from_container(values);
    //}

    // TODO: figure out how to disambiguate this from the std::initializer_list array assignment operator
    //Value& operator=(std::initializer_list<std::pair<Value, Value>> values)
    //{
    //    reset();
    //    type.reset(ValueType::Object);
    //    assign_from_container(values);
    //    return *this;
    //}

    Value(const Value& rhs) { copy_from_internal(rhs); }
    Value& operator=(const Value& rhs) { if (this != &rhs) { copy_from_internal(rhs); } return *this; }

    Value(Value&& rhs) noexcept { move_from_internal(std::move(rhs)); }
    Value& operator=(Value&& rhs) noexcept { if (this != &rhs) { move_from_internal(std::move(rhs)); } return *this; }

    ~Value();

    inline ValueType get_type() const { return type.data; }

    inline bool is_valid() const { return type.data != ValueType::Invalid; }
    inline bool is_invalid() const { return type.data == ValueType::Invalid; }
    inline bool is_null() const { return type.data == ValueType::Null; }
    inline bool is_bool() const { return type.data == ValueType::Bool; }
    inline bool is_number() const { return type.data == ValueType::SignedInteger || type.data == ValueType::UnsignedInteger || type.data == ValueType::Float; }
    inline bool is_integer() const { return type.data == ValueType::SignedInteger || type.data == ValueType::UnsignedInteger; }
    inline bool is_signed_integer() const { return type.data == ValueType::SignedInteger; }
    inline bool is_unsigned_integer() const { return type.data == ValueType::UnsignedInteger; }
    inline bool is_float() const { return type.data == ValueType::Float; }
    inline bool is_string() const { return type.data == ValueType::String; }
    inline bool is_string_inline() const { return type.data == ValueType::String && type.buffer == ByteBufferType::Inline; }
    inline bool is_string_view() const { return type.data == ValueType::String && type.buffer == ByteBufferType::View; }
    inline bool is_string_owned() const { return type.data == ValueType::String && type.buffer == ByteBufferType::Owned; }
    inline bool is_bytes() const { return type.data == ValueType::Bytes; }
    inline bool is_bytes_inline() const { return type.data == ValueType::Bytes && type.buffer == ByteBufferType::Inline; }
    inline bool is_bytes_view() const { return type.data == ValueType::Bytes && type.buffer == ByteBufferType::View; }
    inline bool is_bytes_owned() const { return type.data == ValueType::Bytes && type.buffer == ByteBufferType::Owned; }
    inline bool is_array() const { return type.data == ValueType::Array; }
    inline bool is_object() const { return type.data == ValueType::Object; }

    inline bool_vt as_bool() const { JXC_ASSERT(type.data == ValueType::Bool); return data.value_bool; }
    inline signed_integer_vt as_signed_integer() const { JXC_ASSERT(type.data == ValueType::SignedInteger); return data.value_signed_integer.value; }
    inline unsigned_integer_vt as_unsigned_integer() const { JXC_ASSERT(type.data == ValueType::UnsignedInteger); return data.value_unsigned_integer.value; }
    inline float_vt as_float() const { JXC_ASSERT(type.data == ValueType::Float); return data.value_float.value; }
    inline string_view_vt as_string() const { JXC_ASSERT(type.data == ValueType::String); return as_string_view_unchecked(); }
    inline bytes_view_vt as_bytes() const { JXC_ASSERT(type.data == ValueType::Bytes); return as_bytes_view_unchecked(); }
    inline const array_vt& as_array() const { JXC_ASSERT(type.data == ValueType::Array); static const array_vt empty_arr; return data.value_array ? *data.value_array : empty_arr; }
    inline const object_vt& as_object() const { JXC_ASSERT(type.data == ValueType::Object); static const object_vt empty_obj; return data.value_object ? *data.value_object : empty_obj; }

    template<traits::Integer T = int64_t>
    inline T as_integer() const
    {
        if (type.data == ValueType::SignedInteger)
        {
            return traits::cast_integer_clamped<T>(data.value_signed_integer.value);
        }
        else if (type.data == ValueType::UnsignedInteger)
        {
            return traits::cast_integer_clamped<T>(data.value_unsigned_integer.value);
        }
        JXC_ASSERT(type.data == ValueType::SignedInteger || type.data == ValueType::UnsignedInteger);
        return 0;
    }

    inline std::optional<bool_vt> try_get_bool() const { if (type.data == ValueType::Bool) { return data.value_bool; } return std::nullopt; }
    inline std::optional<signed_integer_vt> try_get_signed_integer() const { if (type.data == ValueType::SignedInteger) { return data.value_signed_integer.value; } return std::nullopt; }
    inline std::optional<unsigned_integer_vt> try_get_unsigned_integer() const { if (type.data == ValueType::UnsignedInteger) { return data.value_unsigned_integer.value; } return std::nullopt; }
    inline std::optional<float_vt> try_get_float() const { if (type.data == ValueType::Float) { return data.value_float.value; } return std::nullopt; }
    inline std::optional<string_view_vt> try_get_string() const { if (type.data == ValueType::String) { return as_string_view_unchecked(); } return std::nullopt; }
    inline std::optional<bytes_view_vt> try_get_bytes() const { if (type.data == ValueType::Bytes) { return as_bytes_view_unchecked(); } return std::nullopt; }

    inline void clear_annotation() { type.anno = annotation.clear(type.anno); }
    bool set_annotation(std::string_view new_anno, std::string* out_anno_parse_error = nullptr);
    bool set_annotation(TokenSpan new_anno_tokens, bool as_view = false);
    bool set_annotation(const OwnedTokenSpan& new_anno_tokens);
    bool set_annotation(OwnedTokenSpan&& new_anno_tokens);
    inline FlexString get_annotation_source() const { return annotation.as_source(type.anno); }

    OwnedTokenSpan get_annotation_tokens() const;

    /// Checks if we fully own our annotation's data
    inline bool is_owned_annotation() const { return type.anno == AnnotationDataType::SourceInline || annotation.is_empty(type.anno) || annotation.have_owned_data(type.anno); }

    /// Checks if we fully own our value's data (only string and bytes types can be unowned)
    inline bool is_owned_value() const
    {
        if (type.data == ValueType::String || type.data == ValueType::Bytes)
        {
            return type.buffer == ByteBufferType::Inline || type.buffer == ByteBufferType::Owned;
        }
        return true;
    }

    /// Checks if this value owns all its data (both the annotation and the value)
    bool is_owned(bool recursive = false) const;

    /// Converts any unowned data in this value to owned data. This copies any annotation, string, or byte views into memory owned by this Value.
    /// If recursive is true, this also applies to all array or object values, recursively.
    Value& convert_to_owned(bool recursive = false);

    /// Converts annotation data to be owned by this value. Does nothing if the annotation data isn't a view.
    Value& convert_to_owned_annotation();

    /// Converts a string view to an owned/inline string. Does nothing if this value isn't a string view.
    Value& convert_to_owned_string();

    /// Converts a bytes view to an owned/inline bytes. Does nothing if this value isn't a bytes view.
    Value& convert_to_owned_bytes();

    /// Requires string or bytes type. Returns a slice of this value.
    Value substr(size_t start, size_t count = invalid_idx) const;

    /// Requires string or bytes type. Returns a slice of this value as a view into this one.
    Value substr_view(size_t start, size_t count = invalid_idx) const;

private:
    void resize_buffer(size_t new_size, size_t max_inline_size);

public:
    inline void resize_string_buffer(size_t new_size)
    {
        JXC_ASSERT(type.data == ValueType::String && type.buffer != ByteBufferType::View);
        resize_buffer(new_size, max_inline_string_len);
    }

    inline void resize_bytes_buffer(size_t new_size)
    {
        JXC_ASSERT(type.data == ValueType::Bytes && type.buffer != ByteBufferType::View);
        resize_buffer(new_size, max_inline_bytes_len);
    }

    template<typename T>
    std::optional<T> cast() const
    {
        if constexpr (traits::Invalid<T>)
        {
            if (type.data == ValueType::Invalid)
            {
                return T{};
            }
        }
        else if constexpr (traits::Null<T>)
        {
            if (type.data == ValueType::Null)
            {
                return T{};
            }
        }
        else if constexpr (traits::Bool<T>)
        {
            switch (type.data)
            {
            case ValueType::Null:
                return false;
            case ValueType::Bool:
                return traits::cast_bool<T>(data.value_bool);
            case ValueType::SignedInteger:
                return static_cast<T>(data.value_signed_integer.value != 0);
            case ValueType::UnsignedInteger:
                return static_cast<T>(data.value_unsigned_integer.value != 0);
            case ValueType::Float:
                return static_cast<T>(data.value_float.value != 0);
            case ValueType::String:
            case ValueType::Bytes:
                return static_cast<T>(bytes_or_string_len_unchecked() > 0);
            case ValueType::Array:
                return static_cast<T>(data.value_array && data.value_array->size() > 0);
            case ValueType::Object:
                return static_cast<T>(data.value_object && data.value_object->size() > 0);
            default:
                break;
            }
        }
        else if constexpr (traits::SignedInteger<T> || traits::UnsignedInteger<T>)
        {
            switch (type.data)
            {
            case ValueType::Null:
                return static_cast<T>(0);
            case ValueType::Bool:
                return static_cast<T>(data.value_bool);
            case ValueType::SignedInteger:
                return traits::cast_integer_clamped<T>(data.value_signed_integer.value);
            case ValueType::UnsignedInteger:
                return traits::cast_integer_clamped<T>(data.value_unsigned_integer.value);
            case ValueType::Float:
                return traits::cast_integer_clamped<T>(static_cast<int64_t>(std::round(data.value_float.value)));
            default:
                break;
            }
        }
        else if constexpr (traits::Float<T>)
        {
            switch (type.data)
            {
            case ValueType::Null:
                return static_cast<T>(0);
            case ValueType::Bool:
                return static_cast<T>(data.value_bool);
            case ValueType::SignedInteger:
                return traits::cast_float_clamped<T>(data.value_signed_integer.value);
            case ValueType::UnsignedInteger:
                return traits::cast_float_clamped<T>(data.value_unsigned_integer.value);
            case ValueType::Float:
                return traits::cast_float_clamped<T>(data.value_float.value);
            default:
                break;
            }
        }
        else if constexpr (std::same_as<T, std::string_view>)
        {
            if (type.data == ValueType::String || type.data == ValueType::Bytes)
            {
                return as_string_view_unchecked();
            }
        }
        else if constexpr (std::constructible_from<T, std::string_view>)
        {
            if (type.data == ValueType::String || type.data == ValueType::Bytes)
            {
                return T(as_string_view_unchecked());
            }
            else
            {
                const auto str = cast_to_string_internal();
                return T(std::string_view(str));
            }
        }
        else if constexpr (std::constructible_from<T, const char*, size_t>)
        {
            if (type.data == ValueType::String || type.data == ValueType::Bytes)
            {
                auto view = as_string_view_unchecked();
                return T(view.data(), view.size());
            }
            else
            {
                const auto str = cast_to_string_internal();
                return T(str.data(), str.size());
            }
        }
        else if constexpr (traits::ConstructibleFromTypedIterator<T, char>)
        {
            if (type.data == ValueType::String || type.data == ValueType::Bytes)
            {
                auto view = as_string_view_unchecked();
                return T(view.begin(), view.end());
            }
            else
            {
                const auto str = cast_to_string_internal();
                return T(str.begin(), str.end());
            }
        }
        else if constexpr (traits::Bytes<T>)
        {
            if constexpr (traits::OwnedByteArray<T>)
            {
                // T is an owned container, so we can return whatever
                switch (type)
                {
                case ValueType::String:
                    // fallthrough
                case ValueType::Bytes:
                {
                    auto view = as_bytes_view_unchecked();
                    return T{ view.data(), view.data() + view.size() };
                }
                case ValueType::Bool:
                {
                    BytesView view = { reinterpret_cast<const uint8_t*>(&data.value_bool), sizeof(bool_vt) };
                    return T{ view.data(), view.data() + view.size() };
                }
                case ValueType::SignedInteger:
                {
                    BytesView view = { reinterpret_cast<const uint8_t*>(&data.value_signed_integer.value), sizeof(signed_integer_vt) };
                    return T{ view.data(), view.data() + view.size() };
                }
                case ValueType::UnsignedInteger:
                {
                    BytesView view = { reinterpret_cast<const uint8_t*>(&data.value_unsigned_integer.value), sizeof(unsigned_integer_vt) };
                    return T{ view.data(), view.data() + view.size() };
                }
                case ValueType::Float:
                {
                    BytesView view = { reinterpret_cast<const uint8_t*>(&data.value_float.value), sizeof(float_vt) };
                    return T{ view.data(), view.data() + view.size() };
                }
                default:
                    break;
                }
            }
            else if constexpr (std::constructible_from<T, const uint8_t*, size_t>)
            {
                // assume T is a BytesView kind of type, so we can _only_ return byte/string data as a view and nothing else
                if (type.data == ValueType::Bytes || type.data == ValueType::String)
                {
                    if (type.buffer == ByteBufferType::Inline)
                    {
                        return T{ data.buffer_inline.bytes, static_cast<size_t>(data.buffer_inline.len) };
                    }
                    else
                    {
                        return T{ data.buffer_ptr.ptr, data.buffer_ptr.len };
                    }
                }
            }
        }

        return std::nullopt;
    }

    string_view_vt get_number_suffix() const;
    void set_number_suffix(string_view_vt new_tag);

    /// If we have owned data, free it and replace it with an empty inline string/bytes
    void reset();

    size_type size() const;

    // equality with another Value
    bool operator==(const Value& rhs) const;
    inline bool operator!=(const Value& rhs) const { return !operator==(rhs); }

    // equality with literal types
    template<traits::Invalid T> inline bool operator==(T rhs) const { return type.data == ValueType::Invalid; }
    template<traits::Invalid T> inline bool operator!=(T rhs) const { return type.data != ValueType::Invalid; }

    template<traits::Null T> inline bool operator==(T rhs) const { return type.data == ValueType::Null; }
    template<traits::Null T> inline bool operator!=(T rhs) const { return type.data != ValueType::Null; }

    template<traits::Bool T> inline bool operator==(T rhs) const { return type.data == ValueType::Bool && traits::cast_bool<bool_vt>(rhs) == data.value_bool; }
    template<traits::Bool T> inline bool operator!=(T rhs) const { return !operator==(rhs); }

    template<traits::Number T>
    inline bool operator==(T rhs) const
    {
        return (type.data == ValueType::SignedInteger && data.value_signed_integer.value == rhs)
            || (type.data == ValueType::UnsignedInteger && data.value_unsigned_integer.value == rhs)
            || (type.data == ValueType::Float && data.value_float.value == rhs);
    }
    template<traits::Number T> inline bool operator!=(T value) const { return !operator==(value); }

    template<traits::StringContainer T> inline bool operator==(const T& value) const { return type.data == ValueType::String && as_string_view_unchecked() == traits::cast_string_to_view(value); }
    template<traits::StringContainer T> inline bool operator!=(const T& value) const { return !operator==(value); }

    inline bool operator==(const char* value) const { return type.data == ValueType::String && as_string_view_unchecked() == traits::cast_string_to_view(value); }
    inline bool operator!=(const char* value) const { return !operator==(value); }

    template<traits::Bytes T> inline bool operator==(const T& value) const { return type.data == ValueType::Bytes && as_bytes_view_unchecked() == traits::cast_bytes_to_view(value); }
    template<traits::Bytes T> inline bool operator!=(const T& value) const { return !operator==(value); }

    template<traits::Array<Value> T>
    inline bool operator==(const T& value) const
    {
        if (type.data != ValueType::Array)
        {
            return false;
        }

        const size_t num_items = data.value_array ? data.value_array->size() : 0;
        if (num_items != value.size())
        {
            return false;
        }
        const array_vt& arr = as_array_unchecked();
        for (size_t i = 0; i < num_items; i++)
        {
            if (arr[i] != value[i])
            {
                return false;
            }
        }
        return true;
    }

    template<traits::Array<Value> T>
    inline bool operator!=(const T& value) const { return !operator==(value); }

    // Negate operator for numeric types.
    // Note that this WILL convert an unsigned integer type to a signed type.
    // In this case, if the value is too large, it will be clamped with no overflow behavior.
    Value operator-() const;

    template<traits::ObjectKey T>
    Value& operator[](T key)
    {
        if constexpr (traits::UnsignedInteger<T>)
        {
            if (type.data == ValueType::Array)
            {
                JXC_ASSERTF(data.value_array && static_cast<size_t>(key) < data.value_array->size(),
                    "Invalid index {} for array of size {}", key, data.value_array ? data.value_array->size() : 0);
                return as_array_unchecked()[static_cast<size_t>(key)];
            }
        }
        else if constexpr (traits::SignedInteger<T>)
        {
            if (type.data == ValueType::Array)
            {
                JXC_ASSERTF(data.value_array != nullptr && data.value_array->size() > 0, "Index {} out of range for array with size 0", key);
                const size_t arr_size = data.value_array->size();
                size_t idx = invalid_idx;
                if (key < 0)
                {
                    // python-style negative index (-1 means the last array element, -2 is second-to-last, etc.)
                    const T reverse_idx = key + static_cast<T>(arr_size);
                    JXC_ASSERTF(reverse_idx >= 0 && reverse_idx < arr_size, "Index {} out of range for array with size {}", key, arr_size);
                    idx = static_cast<size_t>(reverse_idx);
                }
                else
                {
                    idx = static_cast<size_t>(key);
                    JXC_ASSERTF(idx < arr_size, "Index {} out of range for array with size {}", key, arr_size);
                }
                JXC_DEBUG_ASSERT(idx != invalid_idx);
                return as_array_unchecked()[idx];
            }
        }

        if (type.data == ValueType::Object)
        {
            return as_object_unchecked_auto_init()[std::forward<T>(key)];
        }

        JXC_ASSERTF(type.data == ValueType::Array || type.data == ValueType::Object,
            "Cannot use [] operator on value of type {} (requires Array or Object)", value_type_to_string(type.data));
        return *this;
    }

    template<traits::ObjectKey T>
    inline const Value& operator[](T key) const
    {
        return const_cast<Value*>(this)->operator[](std::forward<T>(key));
    }

    Value& at(size_type idx)
    {
        if (type.data == ValueType::Array)
        {
            JXC_ASSERTF(data.value_array && idx < data.value_array->size(),
                "Invalid index {} for array of size {}", idx, data.value_array ? data.value_array->size() : 0);
            return as_array_unchecked()[idx];
        }
        JXC_ASSERTF(type.data == ValueType::Array, "Cannot use at() on {} type (requires Array)", value_type_to_string(type.data));
        return *this;
    }

    inline const Value& at(size_type idx) const { return const_cast<Value*>(this)->at(idx); }

private:
    template<typename KeyT, typename ValT>
    Value& insert_or_assign_internal(KeyT&& key, ValT&& value)
    {
        if (type.data == ValueType::Object)
        {
            if (!data.value_object) { alloc_object(); }
            // result is std::pair<iterator, bool>, where the bool indicates if a new value was inserted
            auto result = data.value_object->insert_or_assign(std::forward<KeyT>(key), std::forward<ValT>(value));
            // return a reference to the inserted value
            return result.first->second;
        }
        JXC_ASSERTF(type.data == ValueType::Object, "Cannot use operator[]() on {} type (requires Object)", value_type_to_string(type.data));
        return *this;
    }

public:
    template<traits::ObjectKey T>
    Value& insert_or_assign(T key, const Value& value) { return insert_or_assign_internal(std::forward<T>(key), value); }

    template<traits::ObjectKey T>
    Value& insert_or_assign(T key, Value&& value) { return insert_or_assign_internal(std::forward<T>(key), std::move(value)); }

    Value& insert_or_assign(Value&& key, const Value& value) { return insert_or_assign_internal(std::move(key), value); }
    Value& insert_or_assign(Value&& key, Value&& value) { return insert_or_assign_internal(std::move(key), std::move(value)); }
    Value& insert_or_assign(const Value& key, const Value& value) { return insert_or_assign_internal(key, value); }
    Value& insert_or_assign(const Value& key, Value&& value) { return insert_or_assign_internal(key, std::move(value)); }

    template<typename Lambda>
    void for_each_key(Lambda&& callback) const
    {
        JXC_ASSERT(type.data == ValueType::Object);
        if (data.value_object)
        {
            for (const auto& pair : *data.value_object)
            {
                callback(pair.first);
            }
        }
    }

    template<typename Lambda>
    void for_each_pair(Lambda&& callback) const
    {
        JXC_ASSERT(type.data == ValueType::Object);
        if (data.value_object)
        {
            for (const auto& pair : *data.value_object)
            {
                callback(pair.first, pair.second);
            }
        }
    }

    inline bool is_valid_index(int64_t idx) const
    {
        return type.data == ValueType::Array && data.value_array != nullptr && idx >= 0 && static_cast<size_t>(idx) < data.value_array->size();
    }

    inline bool contains(const Value& key) const
    {
        return type.data == ValueType::Object && data.value_object && data.value_object->contains(key);
    }

    template<traits::ObjectKey T>
    inline bool remove_key(T key)
    {
        JXC_ASSERT(type.data == ValueType::Object);
        if (!data.value_object)
        {
            return false;
        }
        auto iter = data.value_object->find(std::forward<T>(key));
        if (iter != data.value_object->end())
        {
            data.value_object->erase(iter);
            return true;
        }
        return false;
    }

    inline void push_back(const Value& rhs)
    {
        JXC_ASSERT(type.data == ValueType::Array);
        as_array_unchecked_auto_init().push_back(rhs);
    }

    inline void push_back(Value&& rhs)
    {
        JXC_ASSERT(type.data == ValueType::Array);
        as_array_unchecked_auto_init().push_back(std::move(rhs));
    }

    template<typename... TArgs>
    inline Value& emplace_back(const TArgs&... args)
    {
        JXC_ASSERT(type.data == ValueType::Array);
        return as_array_unchecked_auto_init().emplace_back(args...);
    }

    inline Value& front()
    {
        JXC_ASSERT(type.data == ValueType::Array && data.value_array != nullptr && data.value_array->size() > 0);
        return data.value_array->front();
    }

    inline const Value& front() const
    {
        JXC_ASSERT(type.data == ValueType::Array && data.value_array != nullptr && data.value_array->size() > 0);
        return data.value_array->front();
    }

    inline Value& back()
    {
        JXC_ASSERT(type.data == ValueType::Array && data.value_array != nullptr && data.value_array->size() > 0);
        return data.value_array->back();
    }

    inline const Value& back() const
    {
        JXC_ASSERT(type.data == ValueType::Array && data.value_array != nullptr && data.value_array->size() > 0);
        return data.value_array->back();
    }

    inline void resize(size_t new_array_size)
    {
        JXC_ASSERT(type.data == ValueType::Array);
        as_array_unchecked_auto_init().resize(new_array_size);
    }

    uint64_t hash() const;

    // serialize this value as valid JXC
    std::string to_string(const SerializerSettings& settings = SerializerSettings()) const;

    // get a debug representation of this value
    std::string to_repr(int float_precision = 16, bool fixed_precision = false) const;

    // serialize this value into an existing serializer
    void serialize(Serializer& doc) const;

private:
    // helper function used by cast() when converting to owned string types
    std::string cast_to_string_internal(int float_precision = 16, bool fixed_precision = false) const;

public:

    friend std::ostream& operator<<(std::ostream& os, const Value& val) { return os << val.to_repr(); }
};


// make sure we don't make Value larger by accident by making it a compile error to change its size
JXC_STATIC_ASSERT_SIZEOF(Value, 48);


template<traits::Null T>
inline Value annotated(std::string_view annotation, T, std::string* out_error = nullptr)
{
    Value result(tag::Null{});
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::Bool T>
inline Value annotated(std::string_view annotation, T value, std::string* out_error = nullptr)
{
    Value result(value);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::SignedInteger T>
inline Value annotated(std::string_view annotation, T value, std::string_view suffix = std::string_view{}, std::string* out_error = nullptr)
{
    Value result(value, suffix);
    JXC_DEBUG_ASSERT(result.get_type() == ValueType::SignedInteger);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::UnsignedInteger T>
inline Value annotated(std::string_view annotation, T value, std::string_view suffix = std::string_view{}, std::string* out_error = nullptr)
{
    Value result(value, suffix);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::Float T>
inline Value annotated(std::string_view annotation, T value, std::string_view suffix = std::string_view{}, std::string* out_error = nullptr)
{
    Value result(value, suffix);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::StringContainer T>
inline Value annotated(std::string_view annotation, const T& value, std::string* out_error = nullptr)
{
    Value result(value);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::StringLiteral T>
inline Value annotated(std::string_view annotation, T value, std::string* out_error = nullptr)
{
    Value result(value);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::Bytes T>
inline Value annotated(std::string_view annotation, const T& value, std::string* out_error = nullptr)
{
    Value result(value);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::Array<Value> T>
inline Value annotated(std::string_view annotation, const T& value, std::string* out_error = nullptr)
{
    Value result(value);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<traits::Object<Value, Value> T>
inline Value annotated(std::string_view annotation, const T& value, std::string* out_error = nullptr)
{
    Value result(value);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

template<std::same_as<ValueType> T>
inline Value annotated(std::string_view annotation, T value_type, std::string* out_error = nullptr)
{
    Value result(value_type);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

inline Value annotated_array(std::string_view annotation, std::initializer_list<Value> values, std::string* out_error = nullptr)
{
    Value result(values);
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    return result;
}

inline Value annotated_object(std::string_view annotation, std::initializer_list<std::pair<Value, Value>> pairs, std::string* out_error = nullptr)
{
    Value result(tag::Object{});
    if (!result.set_annotation(annotation, out_error))
    {
        return default_invalid;
    }
    for (const auto& pair : pairs)
    {
        result.insert_or_assign(pair.first, pair.second);
    }
    return result;
}


JXC_BEGIN_NAMESPACE(value_literals)

inline Value operator "" _jxc(long double float_value)
{
    return Value(float_value);
}

inline Value operator "" _jxc(const char* str_value, size_t str_len)
{
    // string literals have static storage, so we can get away with storing them as views here
    return Value(str_value, str_len, Value::AsView{});
}

inline Value operator "" _jxc(unsigned long long uint_value)
{
    // NB. Value has an `operator-` implementation to allow negative values to be constructed correctly
    return Value(uint_value);
}

JXC_END_NAMESPACE(value_literals)


JXC_END_NAMESPACE(jxc)


// defines a literal operator for integers that returns a JXC Value with the specified number suffix
#define JXC_DEFINE_INT_LITERAL_NUMBER_SUFFIX(CPP_SUFFIX, JXC_SUFFIX_STRING) \
    inline ::jxc::Value operator "" CPP_SUFFIX (unsigned long long val) { return ::jxc::Value(val, JXC_SUFFIX_STRING); }


// defines a literal operator for floats that returns a JXC Value with the specified number suffix
#define JXC_DEFINE_FLOAT_LITERAL_NUMBER_SUFFIX(CPP_SUFFIX, JXC_SUFFIX_STRING) \
    inline ::jxc::Value operator "" CPP_SUFFIX (long double val) { return ::jxc::Value(val, JXC_SUFFIX_STRING); }


// defines a literal operator for all number types that returns a JXC Value with the specified number suffix
#define JXC_DEFINE_LITERAL_NUMBER_SUFFIX(CPP_SUFFIX, JXC_SUFFIX_STRING) \
    JXC_DEFINE_INT_LITERAL_NUMBER_SUFFIX(CPP_SUFFIX, JXC_SUFFIX_STRING) \
    JXC_DEFINE_FLOAT_LITERAL_NUMBER_SUFFIX(CPP_SUFFIX, JXC_SUFFIX_STRING)


// std::hash overload for jxc::Value
size_t std::hash<jxc::Value>::operator()(const jxc::Value& value) const
{
    return static_cast<size_t>(value.hash());
}
