#include "jxc_cpp/jxc_document.h"


JXC_BEGIN_NAMESPACE(jxc)


// static
void DocumentSerializer::serialize_null(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).value_null();
}


// static
void DocumentSerializer::serialize_bool(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).value_bool(val.as_bool());
}


// static
void DocumentSerializer::serialize_signed_integer(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).value_int(val.as_signed_integer(), val.get_number_suffix());
}


// static
void DocumentSerializer::serialize_unsigned_integer(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).value_uint(val.as_unsigned_integer(), val.get_number_suffix());
}


// static
void DocumentSerializer::serialize_float(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).value_float(val.as_float(), val.get_number_suffix());
}


// static
void DocumentSerializer::serialize_string(Serializer& doc, const Value& val)
{
    // skip string quotes when we're writing an object key and the value is a valid identifier
    if (doc.is_pending_object_key())
    {
        doc.identifier_or_string(val.as_string());
    }
    else
    {
        doc.annotation(val.get_annotation_source()).value_string(val.as_string());
    }
}


// static
void DocumentSerializer::serialize_bytes(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).value_bytes(val.as_bytes());
}


// static
void DocumentSerializer::serialize_array(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).array_begin();
    const size_t array_sz = val.size();
    for (size_t i = 0; i < array_sz; i++)
    {
        serialize_value(doc, val.at(i));
    }
    doc.array_end();
}


// static
void DocumentSerializer::serialize_object(Serializer& doc, const Value& val)
{
    doc.annotation(val.get_annotation_source()).object_begin();

    val.for_each_pair([&](const Value& key, const Value& item)
    {
        switch (key.get_type())
        {
        case ValueType::Null:
            doc.value_null();
            break;
        case ValueType::Bool:
            doc.value_bool(key.as_bool());
            break;
        case ValueType::SignedInteger:
            doc.value_int(key.as_signed_integer());
            break;
        case ValueType::UnsignedInteger:
            doc.value_uint(key.as_unsigned_integer());
            break;
        case ValueType::String:
            doc.identifier_or_string(key.as_string());
            break;
        case ValueType::Bytes:
            doc.value_bytes(key.as_bytes());
            break;
        default:
            JXC_ASSERTF(false, "{} is not a valid object key type", value_type_to_string(key.get_type()));
            break;
        }
        doc.object_sep();
        serialize_value(doc, item);
    });

    doc.object_end();
}


// static
void DocumentSerializer::serialize_value(Serializer& doc, const Value& val)
{
    switch (val.get_type())
    {
    case ValueType::Null: serialize_null(doc, val); return;
    case ValueType::Bool: serialize_bool(doc, val); return;
    case ValueType::SignedInteger: serialize_signed_integer(doc, val); return;
    case ValueType::UnsignedInteger: serialize_unsigned_integer(doc, val); return;
    case ValueType::Float: serialize_float(doc, val); return;
    case ValueType::String: serialize_string(doc, val); return;
    case ValueType::Bytes: serialize_bytes(doc, val); return;
    case ValueType::Array: serialize_array(doc, val); return;
    case ValueType::Object: serialize_object(doc, val); return;
    default: break;
    }
    JXC_ASSERTF(false, "Invalid value type {}", static_cast<size_t>(val.get_type()));
}



Value detail::ValueParser::parse_number(const Token& tok, TokenView annotation)
{
    JXC_DEBUG_ASSERT(tok.type == TokenType::Number);
    util::NumberTokenSplitResult number;
    if (!util::split_number_token_value(tok, number, parse_error))
    {
        return default_invalid;
    }

    if (number.is_floating_point())
    {
        // float
        double number_value = 0.0;
        if (!util::parse_number(tok, number_value, number, parse_error))
        {
            return default_invalid;
        }
        return make_value_internal(number_value, number.suffix, annotation);
    }
    else
    {
        // int
        int64_t number_value = 0;
        if (!util::parse_number(tok, number_value, number, parse_error))
        {
            return default_invalid;
        }
        return make_value_internal(number_value, number.suffix, annotation);
    }
}


Value detail::ValueParser::parse_bool(const Token& tok, TokenView annotation)
{
    switch (tok.type)
    {
    case TokenType::True:
        return make_value_internal(true, annotation);
    case TokenType::False:
        return make_value_internal(false, annotation);
    default:
        break;
    }
    parse_error = ErrorInfo(jxc::format("Invalid token type {} for boolean value", token_type_to_string(tok.type)), tok.start_idx, tok.end_idx);
    return default_invalid;
}


Value detail::ValueParser::parse_null(const Token& tok, TokenView annotation)
{
    JXC_DEBUG_ASSERT(tok.type == TokenType::Null);
    return make_value_internal(default_null, annotation);
}


Value detail::ValueParser::parse_string(const Token& tok, TokenView annotation)
{
    JXC_DEBUG_ASSERT(tok.type == TokenType::String);
    std::string_view string_value;
    bool is_raw_string = false;
    if (!util::string_token_to_value(tok, string_value, is_raw_string, parse_error))
    {
        return default_invalid;
    }

    // we can return this string as a view instead of a copy
    if (is_raw_string || !util::string_has_escape_chars(string_value))
    {
        static constexpr size_t inline_size_instead_of_view = 8;
        static_assert(inline_size_instead_of_view <= Value::max_inline_string_len, "inline_size must fit into the inline buffer");
        Value result = default_invalid;
        if (string_value.size() <= inline_size_instead_of_view)
        {
            result = Value(string_value, Value::AsInline{});
        }
        else if (try_return_view)
        {
            result = Value(string_value, Value::AsView{});
        }
        else
        {
            result = Value(string_value, Value::AsOwned{});
        }

        if (annotation)
        {
            result.set_annotation(annotation, try_return_view);
        }

        return result;
    }

    Value result(ValueType::String);
    if (annotation)
    {
        result.set_annotation(annotation, try_return_view);
    }

    const size_t req_buf_size = util::get_string_required_buffer_size(string_value, is_raw_string);
    if (req_buf_size == 0)
    {
        return result;
    }

    result.resize_string_buffer(req_buf_size);
    char* buf_ptr = const_cast<char*>(result.as_string().data());

    // string is not raw and has escape chars, so we need to parse the escape characters
    JXC_DEBUG_ASSERT(!is_raw_string || util::string_has_escape_chars(string_value));
    size_t num_chars_written = 0;
    if (!util::parse_string_escapes_to_buffer(string_value, tok.start_idx, tok.end_idx, buf_ptr, req_buf_size, num_chars_written, parse_error))
    {
        return default_invalid;
    }

    // trim the string if it turned out we needed less space than we thought
    if (num_chars_written != req_buf_size)
    {
        JXC_DEBUG_ASSERT(num_chars_written < req_buf_size);
        result.resize_string_buffer(num_chars_written);
    }

    return result;
}


Value detail::ValueParser::parse_bytes(const Token& tok, TokenView annotation)
{
    JXC_DEBUG_ASSERT(tok.type == TokenType::ByteString);
    std::vector<uint8_t> result;
    if (!util::parse_bytes_token(tok, result, parse_error))
    {
        return default_invalid;
    }
    return make_value_internal(std::move(result), annotation);
}


Value detail::ValueParser::parse_array(TokenView annotation)
{
    JXC_DEBUG_ASSERT(parser.value().type == ElementType::BeginArray);
    Value result = make_value_internal(default_array, annotation);
    while (parser.next())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndArray)
        {
            break;
        }
        else
        {
            result.push_back(parse_value_internal(parser.value()));
        }
    }
    return result;
}


Value detail::ValueParser::parse_expression_as_string(TokenView annotation)
{
    JXC_DEBUG_ASSERT(parser.value().type == ElementType::BeginExpression);
    const size_t expr_start_idx = parser.value().token.start_idx;

    size_t expr_inner_start_idx = invalid_idx;
    size_t expr_inner_end_idx = invalid_idx;
    while (parser.next())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndExpression)
        {
            break;
        }

        if (expr_inner_start_idx == invalid_idx)
        {
            expr_inner_start_idx = ele.token.start_idx;
        }

        if (element_is_expression_value_type(ele.type))
        {
            expr_inner_end_idx = ele.token.end_idx;
        }
        else
        {
            parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
                ele.token.start_idx, ele.token.end_idx);
            return default_invalid;
        }
    }

    if (expr_inner_start_idx != invalid_idx && expr_inner_end_idx != invalid_idx && expr_inner_end_idx >= expr_inner_start_idx)
    {
        std::string_view expr_inner_view = parser.get_buffer().substr(expr_inner_start_idx, expr_inner_end_idx - expr_inner_start_idx);
        return make_value_internal(expr_inner_view, annotation);
    }

    parse_error = ErrorInfo("Failed to get valid source string span for expression", expr_start_idx, parser.value().token.end_idx);
    return default_invalid;
}


Value detail::ValueParser::parse_expression_as_array(TokenView annotation)
{
    JXC_DEBUG_ASSERT(parser.value().type == ElementType::BeginExpression);
    Value result = make_value_internal(default_array, annotation);

    while (parser.next())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndExpression)
        {
            break;
        }

        switch (ele.type)
        {
        case ElementType::Null:
            result.push_back(default_null);
            break;
        case ElementType::Bool:
            JXC_DEBUG_ASSERT(ele.token.type == TokenType::True || ele.token.type == TokenType::False);
            result.push_back((ele.token.type == TokenType::True) ? Value(true) : Value(false));
            break;
        case ElementType::Number:
            result.push_back(parse_number(ele.token, TokenView{}));
            break;
        case ElementType::String:
            result.push_back(parse_string(ele.token, TokenView{}));
            break;
        case ElementType::Bytes:
            result.push_back(parse_bytes(ele.token, TokenView{}));
            break;
        case ElementType::ExpressionToken:
            result.push_back(ele.token.value.as_view());
            break;
        case ElementType::Comment:
            // ignore comments for this mode
            break;
        default:
            parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
                ele.token.start_idx, ele.token.end_idx);
            return default_invalid;
        }
    }

    return result;
}


Value detail::ValueParser::parse_key(const Token& tok)
{
    switch (tok.type)
    {
    case TokenType::Identifier:
        return try_return_view ? Value(tok.value, Value::AsView{}) : Value(tok.value);
    case TokenType::True:
        return Value(true);
    case TokenType::False:
        return Value(false);
    case TokenType::Null:
        return Value(default_null);
    case TokenType::Number:
    {
        std::string_view int_key_suffix;
        if (util::is_number_token_negative(tok))
        {
            uint64_t unsigned_int_key_value = 0;
            if (util::parse_number_object_key<uint64_t>(tok, unsigned_int_key_value, parse_error, &int_key_suffix))
            {
                return Value(unsigned_int_key_value, int_key_suffix);
            }
        }
        else
        {
            int64_t int_key_value = 0;
            if (util::parse_number_object_key<int64_t>(tok, int_key_value, parse_error, &int_key_suffix))
            {
                return Value(int_key_value, int_key_suffix);
            }
        }
        return default_invalid;
    }
    case TokenType::String:
    {
        Value result = parse_string(tok, TokenView{});
        if (!result.is_valid())
        {
            return default_invalid;
        }
        return result;
    }
    case TokenType::ByteString:
    {
        std::vector<uint8_t> bytes_key_value;
        if (!util::parse_bytes_token(tok, bytes_key_value, parse_error))
        {
            return default_invalid;
        }
        return Value(bytes_key_value);
    }
    default:
        break;
    }

    parse_error = ErrorInfo(jxc::format("Invalid token for object key: {}", tok.to_repr()), tok.start_idx, tok.end_idx);
    return default_invalid;
}


Value detail::ValueParser::parse_object(TokenView annotation)
{
    JXC_DEBUG_ASSERT(parser.value().type == ElementType::BeginObject);
    Value result = make_value_internal(default_object, annotation);
    while (true)
    {
        if (!parser.next())
        {
            return default_invalid;
        }
        const Element& key_ele = parser.value();
        if (key_ele.type == ElementType::EndObject)
        {
            break;
        }

        JXC_DEBUG_ASSERT(key_ele.type == ElementType::ObjectKey);
        Value key = parse_key(key_ele.token);
        if (key.is_invalid())
        {
            JXC_DEBUG_ASSERT(parse_error.is_err);
            return default_invalid;
        }

        if (!parser.next())
        {
            return default_invalid;
        }

        result.insert_or_assign(std::move(key), parse_value_internal(parser.value()));
    }
    return result;
}


Value detail::ValueParser::parse_value(ElementType element_type, const Token& tok, TokenView annotation, bool expr_as_string)
{
    switch (element_type)
    {
    case ElementType::Invalid: return default_invalid;
    case ElementType::Number: return parse_number(tok, annotation);
    case ElementType::Bool: return parse_bool(tok, annotation);
    case ElementType::Null: return parse_null(tok, annotation);
    case ElementType::Bytes: return parse_bytes(tok, annotation);
    case ElementType::String: return parse_string(tok, annotation);
    case ElementType::BeginArray: return parse_array(annotation);
    case ElementType::BeginExpression: return expr_as_string ? parse_expression_as_string(annotation) : parse_expression_as_array(annotation);
    case ElementType::BeginObject: return parse_object(annotation);
    default: break;
    }
    return default_invalid;
}


Value detail::ValueParser::parse(const Element& ele)
{
    return parse_value_internal(ele);
}



Document::Document(std::string_view in_buffer)
    : buffer(std::string(in_buffer))
    , parser(buffer)
{
    annotation_cache.emplace(std::string(), std::vector<Token>());
}


TokenView Document::copy_annotation(TokenView anno)
{
    if (anno.size() == 0)
    {
        return TokenView{};
    }

    auto make_vector = [](TokenView span) -> std::vector<Token>
    {
        std::vector<Token> result;
        result.reserve(span.size());
        for (size_t i = 0; i < span.size(); i++)
        {
            result.push_back(span[i]);
        }
        return result;
    };

    FlexString key = anno.source();
    if (key.size() > 0)
    {
        auto iter = annotation_cache.find(key);
        if (iter != annotation_cache.end())
        {
            std::vector<Token>& arr = iter->second;
            JXC_DEBUG_ASSERT(arr.size() > 0);
            return TokenView{ arr[0], arr.size() };
        }
        else
        {
            annotation_cache.insert(std::pair{ std::string(key.as_view()), make_vector(anno) });
            std::vector<Token>& arr = annotation_cache[key];
            return TokenView{ arr[0], arr.size() };
        }
    }

    return TokenView{};
}


bool Document::advance()
{
    while (parser.next())
    {
        if (parser.value().type == ElementType::Comment)
        {
            continue;
        }
        return true;
    }
    return false;
}


Value Document::parse()
{
    if (!advance())
    {
        return default_invalid;
    }

    auto value_parser = detail::ValueParser(parser, err, true,
        [this](detail::ValueParser& p, ElementType ele_type, const Token& tok, TokenView anno)
        {
            // For annotations, we copy them once into a buffer owned by the Document,
            // then store a view into that owned copy in the value itself.
            return p.parse_value(ele_type, tok, copy_annotation(anno));
        });

    return value_parser.parse(parser.value());
}


Value Document::parse_to_owned()
{
    if (!advance())
    {
        return default_invalid;
    }

    auto value_parser = detail::ValueParser(parser, err, false,
        [this](detail::ValueParser& p, ElementType ele_type, const Token& tok, TokenView anno)
        {
            return p.parse_value(ele_type, tok, anno);
        });

    return value_parser.parse(parser.value());
}



Value parse(std::string_view jxc_string, ErrorInfo& out_error, bool try_return_view)
{
    JumpParser parser(jxc_string);
    if (!parser.next())
    {
        return default_invalid;
    }

    auto value_parser = detail::ValueParser(parser, out_error, try_return_view,
        [](detail::ValueParser& p, ElementType ele_type, const Token& tok, TokenView anno) -> Value
        {
            return p.parse_value(ele_type, tok, anno);
        });
    
    return value_parser.parse(parser.value());
}


std::string serialize(const Value& val, const SerializerSettings& settings)
{
    DocumentSerializer ar(settings);
    ar.serialize(val);
    return ar.to_string();
}


JXC_END_NAMESPACE(jxc)
