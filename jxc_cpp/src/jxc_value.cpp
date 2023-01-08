#include "jxc_cpp/jxc_value.h"
#include "jxc_cpp/jxc_document.h"


JXC_BEGIN_NAMESPACE(jxc)

const char* value_type_to_string(ValueType type)
{
    switch (type)
    {
    case JXC_ENUMSTR(ValueType, Invalid);
    case JXC_ENUMSTR(ValueType, Null);
    case JXC_ENUMSTR(ValueType, Bool);
    case JXC_ENUMSTR(ValueType, SignedInteger);
    case JXC_ENUMSTR(ValueType, UnsignedInteger);
    case JXC_ENUMSTR(ValueType, Float);
    case JXC_ENUMSTR(ValueType, String);
    case JXC_ENUMSTR(ValueType, Bytes);
    case JXC_ENUMSTR(ValueType, Array);
    case JXC_ENUMSTR(ValueType, Object);
    default:
        break;
    }
    return "INVALID";
}


JXC_BEGIN_NAMESPACE(mem)

template<typename T, typename AllocatorType = std::allocator<T>>
static T* create(size_t count = 1)
{
    static_assert(std::is_default_constructible_v<T>, "mem::create requires a default-constructible type");
    JXC_DEBUG_ASSERT(count > 0);

    // borrowed from nlohmann_json (MIT licensed) - exception-safe allocation method
    AllocatorType alloc;
    using AllocatorTraits = std::allocator_traits<AllocatorType>;
    auto deleter = [&](T* ptr)
    {
        AllocatorTraits::deallocate(alloc, ptr, count);
    };
    std::unique_ptr<T, decltype(deleter)> obj(AllocatorTraits::allocate(alloc, count), deleter);

    if constexpr (std::is_trivially_default_constructible_v<T>)
    {
        // just zero out the buffer
        std::memset(obj.get(), 0, sizeof(T) * count);
    }
    else
    {
        // run the type's constructor
        AllocatorTraits::construct(alloc, obj.get());
    }

    JXC_ASSERT(obj != nullptr);
    return obj.release();
}

template<typename T, typename AllocatorType = std::allocator<T>>
static void destroy(T* ptr, size_t count = 1)
{
    JXC_DEBUG_ASSERT(count > 0);
    AllocatorType alloc;
    if constexpr (!std::is_trivially_constructible_v<T>)
    {
        std::allocator_traits<decltype(alloc)>::destroy(alloc, ptr);
    }
    std::allocator_traits<decltype(alloc)>::deallocate(alloc, ptr, count);
}

JXC_END_NAMESPACE(mem)


// static
uint8_t* Value::create_buffer(size_t buf_len)
{
    return mem::create<uint8_t, byte_allocator_t>(buf_len);
}


// static
void Value::destroy_buffer(uint8_t* buf, size_t buf_len)
{
    mem::destroy<uint8_t, byte_allocator_t>(buf, buf_len);
}


void Value::alloc_buffer_from(string_view_vt val)
{
    JXC_DEBUG_ASSERT(val.data() != nullptr && val.size() > 0);
    const size_t buf_size = val.size();
    uint8_t* buf = create_buffer(buf_size);
    JXC_STRNCPY((char*)buf, buf_size, val.data(), buf_size);
    data.buffer_ptr.ptr = buf;
    data.buffer_ptr.len = buf_size;
}


void Value::alloc_buffer_from(bytes_view_vt val)
{
    JXC_DEBUG_ASSERT(val.data() != nullptr && val.size() > 0);
    const size_t buf_size = val.size();
    uint8_t* buf = create_buffer(buf_size);
    JXC_MEMCPY(buf, buf_size, val.data(), buf_size);
    data.buffer_ptr.ptr = buf;
    data.buffer_ptr.len = buf_size;
}


void Value::free_buffer()
{
    JXC_DEBUG_ASSERT(data.buffer_ptr.ptr != nullptr);
    destroy_buffer(data.buffer_ptr.ptr, data.buffer_ptr.len);
    data.buffer_ptr.ptr = nullptr;
    data.buffer_ptr.len = 0;
}


Value::array_vt& Value::alloc_array()
{
    data.value_array = mem::create<array_vt, array_allocator_t>();
    return *data.value_array;
}


void Value::free_array()
{
    JXC_DEBUG_ASSERT(data.value_array != nullptr);
    mem::destroy<array_vt, array_allocator_t>(data.value_array);
    data.value_array = nullptr;
}


Value::object_vt& Value::alloc_object()
{
    data.value_object = mem::create<object_vt, object_allocator_t>();
    return *data.value_object;
}


void Value::free_object()
{
    JXC_DEBUG_ASSERT(data.value_object != nullptr);
    mem::destroy<object_vt, object_allocator_t>(data.value_object);
    data.value_object = nullptr;
}


void Value::copy_from_internal(const Value& rhs)
{
    JXC_DEBUG_ASSERT(&rhs != this);

    // if we have owned data, we need to free it first
    reset();

    type = rhs.type;

    type.anno = annotation.copy_from(type.anno, rhs.type.anno, rhs.annotation);

    switch (type.data)
    {
    case ValueType::Invalid:
        break;
    case ValueType::Null:
        break;
    case ValueType::Bool:
        data.value_bool = rhs.data.value_bool;
        break;
    case ValueType::SignedInteger:
        data.value_signed_integer.set(rhs.data.value_signed_integer.value, rhs.data.value_signed_integer.get_tag());
        break;
    case ValueType::UnsignedInteger:
        data.value_unsigned_integer.set(rhs.data.value_unsigned_integer.value, rhs.data.value_unsigned_integer.get_tag());
        break;
    case ValueType::Float:
        data.value_float.set(rhs.data.value_float.value, rhs.data.value_float.get_tag());
        break;
    case ValueType::String:
        switch (type.buffer)
        {
        case ByteBufferType::Inline:
            data.set_buffer_inline(rhs.data.get_buffer_inline_as_string_view());
            break;
        case ByteBufferType::View:
            data.set_buffer_ptr(rhs.data.get_buffer_ptr_as_string_view());
            break;
        case ByteBufferType::Owned:
            alloc_buffer_from(rhs.data.get_buffer_ptr_as_string_view());
            break;
        }
        break;
    case ValueType::Bytes:
        switch (type.buffer)
        {
        case ByteBufferType::Inline:
            data.set_buffer_inline(rhs.data.get_buffer_inline_as_bytes_view());
            break;
        case ByteBufferType::View:
            data.set_buffer_ptr(rhs.data.get_buffer_ptr_as_bytes_view());
            break;
        case ByteBufferType::Owned:
            alloc_buffer_from(rhs.data.get_buffer_ptr_as_bytes_view());
            break;
        }
        break;
    case ValueType::Array:
        if (rhs.data.value_array != nullptr && rhs.data.value_array->size() > 0)
        {
            array_vt& arr = alloc_array();
            arr = rhs.as_array_unchecked();
        }
        else
        {
            data.value_array = nullptr;
        }
        break;
    case ValueType::Object:
        if (rhs.data.value_object != nullptr && rhs.data.value_object->size() > 0)
        {
            object_vt& obj = alloc_object();
            obj = rhs.as_object_unchecked();
        }
        else
        {
            data.value_object = nullptr;
        }
        break;
    default:
        break;
    }
}


void Value::move_from_internal(Value&& rhs)
{
    JXC_DEBUG_ASSERT(&rhs != this);

    reset();
    type = rhs.type;
    type.anno = annotation.move_from(type.anno, rhs.type.anno, std::move(rhs.annotation));

    auto move_byte_buffer = [this, &rhs]()
    {
        // move-specific logic for owned string/bytes
        JXC_DEBUG_ASSERT(&rhs != this);
        JXC_DEBUG_ASSERT(rhs.type.data == ValueType::String || rhs.type.data == ValueType::Bytes);
        data.buffer_ptr.ptr = rhs.data.buffer_ptr.ptr;
        data.buffer_ptr.len = rhs.data.buffer_ptr.len;
        rhs.data.buffer_ptr.ptr = nullptr;
        rhs.data.buffer_ptr.len = 0;
    };

    switch (type.data)
    {
    case ValueType::Invalid:
        break;
    case ValueType::Null:
        break;
    case ValueType::Bool:
        data.value_bool = rhs.data.value_bool;
        break;
    case ValueType::SignedInteger:
        data.value_signed_integer.set(rhs.data.value_signed_integer.value, rhs.data.value_signed_integer.get_tag());
        break;
    case ValueType::UnsignedInteger:
        data.value_unsigned_integer.set(rhs.data.value_unsigned_integer.value, rhs.data.value_unsigned_integer.get_tag());
        break;
    case ValueType::Float:
        data.value_float.set(rhs.data.value_float.value, rhs.data.value_float.get_tag());
        break;
    case ValueType::String:
        switch (type.buffer)
        {
        case ByteBufferType::Inline:
            data.set_buffer_inline(rhs.data.get_buffer_inline_as_string_view());
            break;
        case ByteBufferType::View:
            data.set_buffer_ptr(rhs.data.get_buffer_ptr_as_string_view());
            break;
        case ByteBufferType::Owned:
            move_byte_buffer();
            break;
        }
        break;
    case ValueType::Bytes:
        switch (type.buffer)
        {
        case ByteBufferType::Inline:
            data.set_buffer_inline(rhs.data.get_buffer_inline_as_bytes_view());
            break;
        case ByteBufferType::View:
            data.set_buffer_ptr(rhs.data.get_buffer_ptr_as_bytes_view());
            break;
        case ByteBufferType::Owned:
            move_byte_buffer();
            break;
        }
        break;
    case ValueType::Array:
        if (rhs.data.value_array != nullptr && rhs.data.value_array->size() > 0)
        {
            // move array ownership
            data.value_array = rhs.data.value_array;
            rhs.data.value_array = nullptr;
        }
        else
        {
            data.value_array = nullptr;
        }
        break;
    case ValueType::Object:
        if (rhs.data.value_object != nullptr && rhs.data.value_object->size() > 0)
        {
            // move object ownership
            data.value_object = rhs.data.value_object;
            rhs.data.value_object = nullptr;
        }
        else
        {
            data.value_object = nullptr;
        }
        break;
    default:
        break;
    }
}


void Value::assign_from_view(string_view_vt view)
{
    reset();
    type.data = ValueType::String;
    if (view.size() <= max_inline_string_len)
    {
        type.buffer = ByteBufferType::Inline;
        data.set_buffer_inline(view);
    }
    else
    {
        type.buffer = ByteBufferType::Owned;
        alloc_buffer_from(view);
    }
}


void Value::assign_from_view(bytes_view_vt view)
{
    reset();
    type.data = ValueType::Bytes;
    if (view.size() <= max_inline_bytes_len)
    {
        type.buffer = ByteBufferType::Inline;
        data.set_buffer_inline(view);
    }
    else
    {
        type.buffer = ByteBufferType::Owned;
        alloc_buffer_from(view);
    }
}


//static
std::string Value::value_to_string_internal(const Value& val, bool repr_mode, int float_precision, bool fixed_precision)
{
    if (repr_mode)
    {
        return val.to_repr(float_precision, fixed_precision);
    }

    switch (val.type.data)
    {
    case ValueType::Invalid:
        return "invalid";
    case ValueType::Null:
        return "null";
    case ValueType::Bool:
        return val.data.value_bool ? "true" : "false";
    case ValueType::SignedInteger:
        return val.data.value_signed_integer.to_string(float_precision, fixed_precision, true);
    case ValueType::UnsignedInteger:
        return val.data.value_unsigned_integer.to_string(float_precision, fixed_precision, true);
    case ValueType::Float:
        return val.data.value_float.to_string(float_precision, fixed_precision, true);
    case ValueType::String:
        return std::string(val.as_string_view_unchecked());
    case ValueType::Bytes:
        return detail::encode_bytes_to_string(val.as_bytes_view_unchecked());
    case ValueType::Array:
        return array_to_string_internal(val.as_array_unchecked(), repr_mode, float_precision, fixed_precision);
    case ValueType::Object:
        return object_to_string_internal(val.as_object_unchecked(), repr_mode, float_precision, fixed_precision);
    default:
        break;
    }

    return "INVALID";
}


// static
std::string Value::array_to_string_internal(const array_vt& val, bool repr_mode, int float_precision, bool fixed_precision)
{
    std::ostringstream ss;
    ss << '[';
    for (size_t i = 0; i < val.size(); i++)
    {
        if (i > 0)
        {
            ss << ", ";
        }
        ss << value_to_string_internal(val[i], repr_mode, float_precision, fixed_precision);
    }
    ss << ']';
    return ss.str();
}


// static
std::string Value::object_to_string_internal(const object_vt& val, bool repr_mode, int float_precision, bool fixed_precision)
{
    std::ostringstream ss;
    ss << '{';
    bool first = true;
    for (const auto& pair : val)
    {
        if (first)
        {
            first = !first;
        }
        else
        {
            ss << ", ";
        }
        ss << value_to_string_internal(pair.first, repr_mode, float_precision, fixed_precision)
            << ": " << value_to_string_internal(pair.second, repr_mode, float_precision, fixed_precision);
    }
    ss << '}';
    return ss.str();
}


Value::Value(ValueType in_type)
    : type(Metadata::init(in_type))
{
    switch (in_type)
    {
    case ValueType::Bool:
        data.value_bool = false;
        break;
    case ValueType::SignedInteger:
        data.value_signed_integer.set(0);
        break;
    case ValueType::UnsignedInteger:
        data.value_unsigned_integer.set(0);
        break;
    case ValueType::Float:
        data.value_float.set(0);
        break;
    case ValueType::String:
        data.clear_buffer_inline();
        break;
    case ValueType::Bytes:
        data.clear_buffer_inline();
        break;

        // all other value types have no storage
    default:
        break;
    }
}


Value::~Value()
{
    if (type.buffer == ByteBufferType::Owned && (type.data == ValueType::String || type.data == ValueType::Bytes) && data.buffer_ptr.ptr != nullptr)
    {
        free_buffer();
    }
    else if (type.data == ValueType::Array && data.value_array != nullptr)
    {
        free_array();
    }
    else if (type.data == ValueType::Object && data.value_object != nullptr)
    {
        free_object();
    }

    // clear out annotation data if we own any
    annotation.clear(type.anno);
}


bool Value::set_annotation(std::string_view new_anno, std::string* out_anno_parse_error)
{
    if (new_anno.size() == 0)
    {
        type.anno = annotation.clear(type.anno);
        return true;
    }
    else if (new_anno.size() <= detail::AnnotationData::max_inline_annotation_source_len && is_valid_identifier(new_anno))
    {
        type.anno = annotation.assign_source_inline(type.anno, new_anno);
        return true;
    }
    else if (auto toks = OwnedTokenSpan::parse_annotation(new_anno, out_anno_parse_error))
    {
        type.anno = annotation.assign_tokens_owned_move(type.anno, std::move(toks.value()));
        return true;
    }
    return false;
}


bool Value::set_annotation(TokenSpan new_anno_tokens, bool as_view)
{
    if (as_view)
    {
        type.anno = annotation.assign_tokens_view(type.anno, new_anno_tokens);
    }
    else if (new_anno_tokens.size() == 1 && new_anno_tokens[0].value.size() <= detail::AnnotationData::max_inline_annotation_source_len)
    {
        type.anno = annotation.assign_source_inline(type.anno, new_anno_tokens[0].value.as_view());
    }
    else
    {
        type.anno = annotation.assign_tokens_owned_copy_from_view(type.anno, new_anno_tokens);
    }
    return true;
}


bool Value::set_annotation(const OwnedTokenSpan& new_anno_tokens)
{
    if (new_anno_tokens.size() == 1 && new_anno_tokens[0].value.size() <= detail::AnnotationData::max_inline_annotation_source_len
        && new_anno_tokens.src == new_anno_tokens[0].value)
    {
        type.anno = annotation.assign_source_inline(type.anno, new_anno_tokens[0].value.as_view());
    }
    else
    {
        type.anno = annotation.assign_tokens_owned_copy_from_view(type.anno, TokenSpan(const_cast<Token&>(new_anno_tokens[0]), new_anno_tokens.size()));
    }
    return true;
}


bool Value::set_annotation(OwnedTokenSpan&& new_anno_tokens)
{
    if (new_anno_tokens.size() == 1 && new_anno_tokens[0].value.size() <= detail::AnnotationData::max_inline_annotation_source_len
        && new_anno_tokens.src == new_anno_tokens[0].value)
    {
        type.anno = annotation.assign_source_inline(type.anno, new_anno_tokens[0].value.as_view());
    }
    else
    {
        type.anno = annotation.assign_tokens_owned_move(type.anno, std::move(new_anno_tokens));
    }
    return true;
}


OwnedTokenSpan Value::get_annotation_tokens() const
{
    if (type.anno == AnnotationDataType::TokenSpanOwned || type.anno == AnnotationDataType::TokenSpanView)
    {
        auto result = OwnedTokenSpan(annotation.as_token_span(type.anno));
        result.src = FlexString::make_owned(annotation.as_source(type.anno));
        return result;
    }
    else if (type.anno == AnnotationDataType::SourceInline)
    {
        auto tok_view = annotation.as_source_inline_view_unchecked();
        OwnedTokenSpan span;
        JXC_DEBUG_ASSERT(tok_view.size() <= FlexString::max_inline_len);
        span.src = FlexString(tok_view, FlexString::ConstructInline{});
        span.tokens.push(Token(TokenType::Identifier, invalid_idx, invalid_idx,
            FlexString(tok_view, FlexString::ConstructInline{})));
        return span;
    }
    return OwnedTokenSpan();
}


bool Value::is_owned(bool recursive) const
{
    if (!is_owned_annotation())
    {
        return false;
    }

    switch (type.data)
    {
    case ValueType::String:
        // fallthrough
    case ValueType::Bytes:
        return is_owned_value();
    case ValueType::Array:
        if (recursive && data.value_array != nullptr)
        {
            for (const auto& item : as_array_unchecked())
            {
                if (!item.is_owned(recursive))
                {
                    return false;
                }
            }
        }
        break;
    case ValueType::Object:
        if (recursive && data.value_object != nullptr)
        {
            for (const auto& pair : as_object_unchecked())
            {
                if (!pair.first.is_owned(recursive) || !pair.second.is_owned(recursive))
                {
                    return false;
                }
            }
        }
        break;
    default:
        break;
    }

    return true;
}


Value& Value::convert_to_owned(bool recursive)
{
    // if annotation data is a view, convert it to owned
    if (type.anno == AnnotationDataType::TokenSpanView)
    {
        type.anno = annotation.convert_view_to_owned(type.anno);
    }

    // convert values to owned
    switch (type.data)
    {
    case ValueType::String:
        if (type.buffer == ByteBufferType::View)
        {
            convert_to_owned_string();
        }
        break;
    case ValueType::Bytes:
        if (type.buffer == ByteBufferType::View)
        {
            convert_to_owned_bytes();
        }
        break;
    case ValueType::Array:
        if (recursive && data.value_array != nullptr)
        {
            for (auto& item : as_array_unchecked())
            {
                item.convert_to_owned(recursive);
            }
        }
        break;
    case ValueType::Object:
        if (recursive && data.value_object != nullptr)
        {
            object_vt& obj = as_object_unchecked();
            for (auto& pair : obj.values())
            {
                // NB. Need to be careful modifying object keys here - we don't want to change their hash value by accident.
                const_cast<Value&>(pair.first).convert_to_owned(recursive);
                const_cast<Value&>(pair.second).convert_to_owned(recursive);
            }
        }
        break;
    default:
        break;
    }

    return *this;
}


Value& Value::convert_to_owned_annotation()
{
    if (type.anno == AnnotationDataType::TokenSpanView)
    {
        type.anno = annotation.convert_view_to_owned(type.anno);
    }

    return *this;
}


Value& Value::convert_to_owned_string()
{
    if (type.data != ValueType::String || type.buffer != ByteBufferType::View)
    {
        return *this;
    }

    const size_t buf_len = data.buffer_ptr.len;
    string_view_vt view = { (const char*)data.buffer_ptr.ptr, buf_len };

    if (buf_len <= max_inline_string_len)
    {
        data.clear_buffer_inline();
        JXC_STRNCPY((char*)&data.buffer_inline.bytes[0], buf_len, view.data(), buf_len);
        data.buffer_inline.len = static_cast<uint8_t>(buf_len);
        type.buffer = ByteBufferType::Inline;
    }
    else
    {
        data.buffer_ptr.ptr = create_buffer(buf_len);
        JXC_STRNCPY((char*)data.buffer_ptr.ptr, buf_len, view.data(), buf_len);
        JXC_DEBUG_ASSERT(data.buffer_ptr.len == buf_len);
        type.buffer = ByteBufferType::Owned;
    }

    return *this;
}


Value& Value::convert_to_owned_bytes()
{
    if (type.data != ValueType::Bytes || type.buffer != ByteBufferType::View)
    {
        return *this;
    }

    const size_t buf_len = data.buffer_ptr.len;
    bytes_view_vt view = { data.buffer_ptr.ptr, buf_len };

    if (buf_len <= max_inline_bytes_len)
    {
        data.clear_buffer_inline();
        JXC_MEMCPY(&data.buffer_inline.bytes[0], buf_len, view.data(), buf_len);
        data.buffer_inline.len = static_cast<uint8_t>(buf_len);
        type.buffer = ByteBufferType::Inline;
    }
    else
    {
        data.buffer_ptr.ptr = create_buffer(buf_len);
        JXC_MEMCPY(data.buffer_ptr.ptr, buf_len, view.data(), buf_len);
        JXC_DEBUG_ASSERT(data.buffer_ptr.len == buf_len);
        type.buffer = ByteBufferType::Owned;
    }

    return *this;
}


Value Value::substr(size_t start, size_t count) const
{
    JXC_ASSERT(type.data == ValueType::String || type.data == ValueType::Bytes);

    static_assert(invalid_idx == std::string_view::npos,
        "if these are not equal, then the default value of count would do strange things");

    if (type.data == ValueType::String)
    {
        return Value(as_string_view_unchecked().substr(start, count));
    }
    else
    {
        JXC_DEBUG_ASSERT(type.data == ValueType::Bytes);
        return Value(as_bytes_view_unchecked().slice(start, count));
    }
}


Value Value::substr_view(size_t start, size_t count) const
{
    JXC_ASSERT(type.data == ValueType::String || type.data == ValueType::Bytes);

    static_assert(invalid_idx == std::string_view::npos,
        "if these are not equal, then the default value of count would do strange things");

    if (type.data == ValueType::String)
    {
        return Value(as_string_view_unchecked().substr(start, count), AsView{});
    }
    else
    {
        JXC_DEBUG_ASSERT(type.data == ValueType::Bytes);
        return Value(as_bytes_view_unchecked().slice(start, count), AsView{});
    }
}


void Value::resize_buffer(size_t new_size, size_t max_inline_size)
{
    JXC_DEBUG_ASSERT(type.data == ValueType::String || type.data == ValueType::Bytes);
    if (type.buffer == ByteBufferType::Inline)
    {
        if (new_size == (size_t)data.buffer_inline.len)
        {
            return;
        }
        else if (new_size <= max_inline_size)
        {
            // currently inline and new size fits in the inline buffer - just set the size and we're done
            data.buffer_inline.len = static_cast<uint8_t>(new_size);
        }
        else
        {
            // currently inline and the new size does not fit. need to switch to an allocated buffer.
            auto existing_data_view = as_bytes_view_unchecked();
            uint8_t* buf = create_buffer(new_size);
            if (existing_data_view.size() > 0)
            {
                JXC_MEMCPY(buf, new_size, existing_data_view.data(), existing_data_view.size());
            }
            data.clear();
            data.buffer_ptr.ptr = buf;
            data.buffer_ptr.len = new_size;
            type.buffer = ByteBufferType::Owned;
        }
    }
    else
    {
        JXC_DEBUG_ASSERT(type.buffer == ByteBufferType::Owned);
        if (new_size == data.buffer_ptr.len)
        {
            return;
        }

        auto existing_data_view = data.get_buffer_ptr_as_bytes_view();
        const size_t num_bytes_to_copy = std::min(existing_data_view.size(), new_size);

        if (new_size <= max_inline_size)
        {
            // switch owned to inline
            data.clear();
            if (num_bytes_to_copy > 0)
            {
                JXC_MEMCPY(&data.buffer_inline.bytes[0], max_inline_size, existing_data_view.data(), num_bytes_to_copy);
            }
            data.buffer_inline.len = static_cast<uint8_t>(new_size);
            type.buffer = ByteBufferType::Inline;
        }
        else
        {
            // currently owned, new size also requires owned, so we need to reallocate
            uint8_t* new_buf = create_buffer(new_size);
            if (num_bytes_to_copy > 0)
            {
                JXC_MEMCPY(new_buf, new_size, existing_data_view.data(), num_bytes_to_copy);
            }
            data.buffer_ptr.ptr = new_buf;
            data.buffer_ptr.len = new_size;
        }

        // destroy original buffer
        destroy_buffer(const_cast<uint8_t*>(existing_data_view.data()), existing_data_view.size());
    }
}


Value::string_view_vt Value::get_number_suffix() const
{
    switch (type.data)
    {
    case ValueType::SignedInteger:
        return data.value_signed_integer.get_tag();
    case ValueType::UnsignedInteger:
        return data.value_unsigned_integer.get_tag();
    case ValueType::Float:
        return data.value_float.get_tag();
    default:
        break;
    }
    JXC_ASSERT(is_number());
    return string_view_vt{};
}


void Value::set_number_suffix(string_view_vt new_tag)
{
    JXC_ASSERT(new_tag.size() <= max_number_tag_len);
    switch (type.data)
    {
    case ValueType::SignedInteger:
        data.value_signed_integer.set_tag(new_tag);
        break;
    case ValueType::UnsignedInteger:
        data.value_unsigned_integer.set_tag(new_tag);
        break;
    case ValueType::Float:
        data.value_float.set_tag(new_tag);
        break;
    default:
        break;
    }
    JXC_ASSERT(is_number());
}


void Value::reset()
{
    switch (type.data)
    {
    case ValueType::String:
        // fallthrough
    case ValueType::Bytes:
        if (type.buffer == ByteBufferType::Owned && data.buffer_ptr.ptr != nullptr)
        {
            free_buffer();
        }
        break;
    case ValueType::Array:
        if (data.value_array != nullptr)
        {
            free_array();
        }
        break;
    case ValueType::Object:
        if (data.value_object != nullptr)
        {
            free_object();
        }
        break;
    default:
        break;
    }

    type.reset();
    data.clear();
}


Value::size_type Value::size() const
{
    switch (type.data)
    {
        // can use the exact same logic for string and bytes - they share storage
    case ValueType::String:
        // fallthrough
    case ValueType::Bytes:
        return static_cast<size_type>(bytes_or_string_len_unchecked());
    case ValueType::Array:
        return (data.value_array == nullptr) ? 0 : data.value_array->size();
    case ValueType::Object:
        return (data.value_object == nullptr) ? 0 : data.value_object->size();
    default:
        break;
    }

    return 0;
}


bool Value::operator==(const Value& rhs) const
{
    if (!annotation.eq(type.anno, rhs.type.anno, rhs.annotation))
    {
        return false;
    }

    if (is_number() && rhs.is_number())
    {
        switch (type.data)
        {
        case ValueType::SignedInteger:
            switch (rhs.type.data)
            {
            case ValueType::SignedInteger:
                return data.value_signed_integer.eq(rhs.data.value_signed_integer);
            case ValueType::UnsignedInteger:
                return data.value_signed_integer.eq(rhs.data.value_unsigned_integer);
            case ValueType::Float:
                return data.value_signed_integer.eq(rhs.data.value_float);
            default:
                break;
            }
            break;
        case ValueType::UnsignedInteger:
            switch (rhs.type.data)
            {
            case ValueType::SignedInteger:
                return data.value_unsigned_integer.eq(rhs.data.value_signed_integer);
            case ValueType::UnsignedInteger:
                return data.value_unsigned_integer.eq(rhs.data.value_unsigned_integer);
            case ValueType::Float:
                return data.value_unsigned_integer.eq(rhs.data.value_float);
            default:
                break;
            }
            break;
        case ValueType::Float:
            switch (rhs.type.data)
            {
            case ValueType::SignedInteger:
                return data.value_float.eq(rhs.data.value_signed_integer);
            case ValueType::UnsignedInteger:
                return data.value_float.eq(rhs.data.value_unsigned_integer);
            case ValueType::Float:
                return data.value_float.eq(rhs.data.value_float);
            default:
                break;
            }
            break;
        default:
            break;
        }
        JXC_UNREACHABLE("Unreachable");
        return false;
    }

    if (type.data == rhs.type.data)
    {
        switch (type.data)
        {
        // these types are value-less
        case ValueType::Invalid:
            // fallthrough
        case ValueType::Null:
            return true;
        case ValueType::Bool:
            return data.value_bool == rhs.data.value_bool;
        case ValueType::String:
            return as_string_view_unchecked() == rhs.as_string_view_unchecked();
        case ValueType::Bytes:
            return as_bytes_view_unchecked() == rhs.as_bytes_view_unchecked();
        case ValueType::Array:
            return (data.value_array && rhs.data.value_array)
                ? (as_array_unchecked() == rhs.as_array_unchecked())
                : (data.value_array == rhs.data.value_array);
        case ValueType::Object:
            return (data.value_object && rhs.data.value_object)
                ? (as_object_unchecked() == rhs.as_object_unchecked())
                : (data.value_object == rhs.data.value_object);
        default:
            break;
        }
    }
    return false;
}


Value Value::operator-() const
{
    switch (type.data)
    {
    case ValueType::SignedInteger:
        return Value(-as_signed_integer(), data.value_signed_integer.get_tag());
    case ValueType::UnsignedInteger:
        return Value(-traits::cast_integer_clamped<signed_integer_vt>(as_unsigned_integer()), data.value_unsigned_integer.get_tag());
    case ValueType::Float:
        return Value(-as_float(), data.value_float.get_tag());
    default:
        break;
    }
    JXC_ASSERT(type.data == ValueType::SignedInteger || type.data == ValueType::UnsignedInteger || type.data == ValueType::Float);
    return Value{};
}


JXC_BEGIN_NAMESPACE(tvhash)

JXC_FORCEINLINE uint64_t hash_uint64(uint64_t value)
{
    return ankerl::unordered_dense::detail::wyhash::hash(value);
}


JXC_FORCEINLINE uint64_t reinterpret_signed_to_unsigned(int64_t value)
{
    uint64_t* value_ptr = reinterpret_cast<uint64_t*>(&value);
    return *value_ptr;
}


template<typename T>
inline uint64_t hash_value(const T& value) { return ankerl::unordered_dense::hash<T>()(value); }


template<typename T>
inline uint64_t hash_enum(T enum_value) { return hash_uint64(static_cast<uint64_t>(enum_value)); }


template<typename ValueT, typename EnumT>
inline uint64_t hash_enum_value_pair(EnumT type, const ValueT& value)
{
    uint64_t result = hash_enum<EnumT>(type);
    jxc::detail::hash_combine(result, ankerl::unordered_dense::hash<ValueT>()(value));
    return result;
}


template<typename EnumT>
inline uint64_t hash_enum_hash_pair(EnumT type, uint64_t prehashed_value)
{
    uint64_t result = hash_enum<EnumT>(type);
    jxc::detail::hash_combine(result, prehashed_value);
    return result;
}


static const uint64_t s_hash_invalid = hash_enum(ValueType::Invalid);
static const uint64_t s_hash_null = hash_enum(ValueType::Null);
static const uint64_t s_hash_bool_false = hash_enum_value_pair(ValueType::Bool, false);
static const uint64_t s_hash_bool_true = hash_enum_value_pair(ValueType::Bool, true);
static const uint64_t s_hash_array_empty = hash_enum_hash_pair(ValueType::Array, hash_uint64(0));


inline uint64_t hash_signed_int(int64_t value, std::string_view tag)
{
    uint64_t result = hash_enum(ValueType::SignedInteger);
    jxc::detail::hash_combine(result, hash_uint64(reinterpret_signed_to_unsigned(value)));
    if (tag.size() > 0)
    {
        jxc::detail::hash_combine(result, hash_value(tag));
    }
    return result;
}


inline uint64_t hash_unsigned_int(uint64_t value, std::string_view tag)
{
    uint64_t result = hash_enum(ValueType::UnsignedInteger);
    jxc::detail::hash_combine(result, hash_uint64(value));
    if (tag.size() > 0)
    {
        jxc::detail::hash_combine(result, hash_value(tag));
    }
    return result;
}


inline uint64_t hash_float(double value, std::string_view tag)
{
    uint64_t result = hash_enum(ValueType::Float);
    jxc::detail::hash_combine(result, hash_value(value));
    if (tag.size() > 0)
    {
        jxc::detail::hash_combine(result, hash_value(tag));
    }
    return result;
}


inline uint64_t hash_string(std::string_view value)
{
    return hash_enum_value_pair(ValueType::String, value);
}


static uint64_t hash_bytes(BytesView value)
{
    uint64_t result = hash_enum(ValueType::Bytes);
    const size_t num_bytes = (value.data() == nullptr || value.size() == 0) ? 0 : value.size();
    jxc::detail::hash_combine<uint64_t>(result, hash_uint64(num_bytes));
    if (num_bytes > 0)
    {
        size_t idx = 0;

        // treat each slice of 8 bytes as a uint64 for the purpose of hashing (way less iterations through the byte array)
        if (num_bytes >= 8)
        {
            while (idx < num_bytes)
            {
                const uint64_t* slice = reinterpret_cast<const uint64_t*>(value.data() + idx);
                jxc::detail::hash_combine<uint64_t>(result, hash_uint64(*slice));
                idx += 8;
            }
        }

#if JXC_DEBUG
        size_t num_byte_iters = 0;
#endif

        // take any remaining bytes and hash them individually (this should never have more than 7 iterations)
        while (idx < num_bytes)
        {
            jxc::detail::hash_combine<uint64_t>(result, hash_uint64(static_cast<uint64_t>(value[idx])));
            idx += 1;

#if JXC_DEBUG
            ++num_byte_iters;
#endif
        }

        JXC_DEBUG_ASSERT(num_byte_iters <= 7);
    }
    return result;
}


static uint64_t hash_array(const Value::array_vt* value)
{
    const size_t num_items = (value != nullptr) ? value->size() : 0;
    uint64_t result = hash_enum_hash_pair(ValueType::Array, hash_uint64(num_items));
    if (num_items > 0)
    {
        for (size_t i = 0; i < num_items; i++)
        {
            jxc::detail::hash_combine<uint64_t>(result, (*value)[i].hash());
        }
    }
    return result;
}


static uint64_t hash_object(const Value::object_vt*)
{
    JXC_ASSERTF(false, "Unhashable type: object");
    return 0;
}

JXC_END_NAMESPACE(tvhash)


uint64_t Value::hash() const
{
    switch (type.data)
    {
    case ValueType::Invalid: return tvhash::s_hash_invalid;
    case ValueType::Null: return tvhash::s_hash_null;
    case ValueType::Bool: return data.value_bool ? tvhash::s_hash_bool_true : tvhash::s_hash_bool_false;
    case ValueType::SignedInteger: return tvhash::hash_signed_int(data.value_signed_integer.value, data.value_signed_integer.get_tag());
    case ValueType::UnsignedInteger: return tvhash::hash_unsigned_int(data.value_unsigned_integer.value, data.value_unsigned_integer.get_tag());
    case ValueType::Float: return tvhash::hash_float(data.value_float.value, data.value_float.get_tag());
    case ValueType::String: return tvhash::hash_string(as_string_view_unchecked());
    case ValueType::Bytes: return tvhash::hash_bytes(as_bytes_view_unchecked());
    case ValueType::Array: return tvhash::hash_array(data.value_array);
    case ValueType::Object: return tvhash::hash_object(data.value_object);
    default: break;
    }
    return 0;
}


std::string Value::to_string(const SerializerSettings& settings) const
{
    DocumentSerializer ar(settings);
    ar.serialize(*this);
    return ar.to_string();
}


std::string Value::to_repr(int float_precision, bool fixed_precision) const
{
    auto number_repr = [float_precision, fixed_precision]<typename T, size_t MaxTagLen>(const detail::TaggedNum<T, MaxTagLen>& num) -> std::string
    {
        detail::MiniBuffer<char, 48> buf;
        std::string_view num_string = std::string_view{ buf.data(), num.to_string_in_buffer(buf.data(), buf.capacity(), float_precision, fixed_precision, false) };
        return (num.tag_len == 0) ? std::string(num_string) : jxc::format("{}, tag={}", num_string, detail::debug_string_repr(num.get_tag()));
    };

    auto anno_repr = [this]() -> std::string
    {
        if (type.anno == AnnotationDataType::SourceInline)
        {
            auto anno_src = annotation.as_source_inline_view_unchecked();
            if (anno_src.size() > 0)
            {
                return jxc::format(", anno={}", detail::debug_string_repr(anno_src));
            }
        }
        else if (type.anno == AnnotationDataType::TokenSpanOwned || type.anno == AnnotationDataType::TokenSpanView)
        {
            auto anno_span = annotation.as_token_span(type.anno);
            if (anno_span.size() > 0)
            {
                return jxc::format(", anno={}", anno_span.to_repr());
            }
        }
        return std::string{};
    };

    const char* value_type_str = value_type_to_string(type.data);
    switch (type.data)
    {
    case ValueType::Null:
        return jxc::format("Value({}{})", value_type_str, anno_repr());
    case ValueType::Bool:
        return jxc::format("Value({}, {}{})", value_type_str, (data.value_bool ? "true" : "false"), anno_repr());
    case ValueType::SignedInteger:
        return jxc::format("Value({}, {}{})", value_type_str, number_repr(data.value_signed_integer), anno_repr());
    case ValueType::UnsignedInteger:
        return jxc::format("Value({}, {}{})", value_type_str, number_repr(data.value_unsigned_integer), anno_repr());
    case ValueType::Float:
        return jxc::format("Value({}, {}{})", value_type_str, number_repr(data.value_float), anno_repr());
    case ValueType::String:
        return jxc::format("Value({}, {}{})", value_type_str, detail::debug_string_repr(as_string_view_unchecked()), anno_repr());
    case ValueType::Bytes:
        return jxc::format("Value({}, {}{})", value_type_str, detail::debug_bytes_repr(as_bytes_view_unchecked()), anno_repr());
    case ValueType::Array:
        return jxc::format("Value({}, {}{})", value_type_str,
            data.value_array ? array_to_string_internal(as_array_unchecked(), true, float_precision, fixed_precision) : "[]", anno_repr());
    case ValueType::Object:
        return jxc::format("Value({}, {}{})", value_type_str,
            data.value_object ? object_to_string_internal(as_object_unchecked(), true, float_precision, fixed_precision) : "{}", anno_repr());
    default:
        break;
    }
    return "Value(Invalid)";
}


std::string Value::cast_to_string_internal(int float_precision, bool fixed_precision) const
{
    switch (type.data)
    {
    case ValueType::Invalid:
        return "invalid";
    case ValueType::Null:
        return "null";
    case ValueType::Bool:
        return data.value_bool ? "true" : "false";
    case ValueType::SignedInteger:
        return data.value_signed_integer.to_string(float_precision, fixed_precision, true);
    case ValueType::UnsignedInteger:
        return data.value_unsigned_integer.to_string(float_precision, fixed_precision, true);
    case ValueType::Float:
        return data.value_float.to_string(float_precision, fixed_precision, true);
    case ValueType::String:
        return std::string(as_string_view_unchecked());
    case ValueType::Bytes:
        return detail::encode_bytes_to_string(as_bytes_view_unchecked());
    case ValueType::Array:
        return data.value_array ? array_to_string_internal(as_array_unchecked(), false, float_precision, fixed_precision) : "[]";
    case ValueType::Object:
        return data.value_object ? object_to_string_internal(as_object_unchecked(), false, float_precision, fixed_precision) : "{}";
    default:
        break;
    }
    return "INVALID_VALUE_TYPE";
}

JXC_END_NAMESPACE(jxc)
