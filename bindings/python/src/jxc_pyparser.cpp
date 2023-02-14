#include "jxc_pyparser.h"
#include "jxc/jxc_util.h"


#define JXC_PYTHON_PARSE_ERROR(PY_ERR_TYPE, ERRINFO) do { \
    ErrorInfo _err_ = (ERRINFO); \
    JXC_DEBUG_ASSERT(_err_.is_err); \
    _err_.get_line_and_col_from_buffer(buf); \
    throw PY_ERR_TYPE(_err_.to_string(buf)); \
} while(0)


#define JXC_UNKNOWN_NUMBER_SUFFIX_ERROR(ELE) do { \
    parse_error = ErrorInfo(jxc::format("Unknown number suffix {}", (ELE).annotation.source()), (ELE).token.start_idx, (ELE).token.end_idx); \
    JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error); \
} while(0)


#define JXC_UNKNOWN_ANNOTATION_ERROR(ELE) do { \
    parse_error = ErrorInfo(jxc::format("Unknown annotation {}", (ELE).annotation.source()), (ELE).token.start_idx, (ELE).token.end_idx); \
    JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error); \
} while(0)


namespace jxc
{


//static
std::optional<ValueConstructor> ValueConstructor::from_python(py::object value)
{
    if (value.is_none())
    {
        return std::nullopt;
    }

    ValueConstructor result;

    if (py::isinstance<py::tuple>(value))
    {
        py::tuple value_tup = value;
        const size_t sz = value_tup.size();
        if (sz >= 1)
        {
            result.obj = value_tup[0];
        }

        switch (sz)
        {
        case 1:
            break;

        case 2:
            // allow a 2-tuple in the form (callable, expr_parse_mode) or (callable, class_construct_mode)
            if (py::isinstance<ExpressionParseMode>(value_tup[1]))
            {
                result.expr_parse_mode = py::cast<ExpressionParseMode>(value_tup[1]);
            }
            else
            {
                result.construct_mode = py::cast<ClassConstructMode>(value_tup[1]);
            }
            break;
        
        case 3:
            result.construct_mode = py::cast<ClassConstructMode>(value_tup[1]);
            result.expr_parse_mode = py::cast<ExpressionParseMode>(value_tup[2]);
            break;
        
        default:
            throw py::value_error(jxc::format("Expected tuple of length 0..3, got {}", py::cast<std::string>(py::repr(value))));
        }
    }
    else
    {
        result.obj = value;
    }

    result.is_type = py::isinstance<py::type>(result.obj);
    result.has_inline_decode_method = result.is_type && py::hasattr(result.obj, "_jxc_decode");

    return result;
}


py::object ValueConstructor::construct(py::object value, const Element& ele, ErrorInfo& out_error) const
{
    JXC_DEBUG_ASSERT(!out_error.is_err);
    if (is_type)
    {
        if (has_inline_decode_method)
        {
            return obj.attr("_jxc_decode")(value);
        }
        else
        {
            // figure out how to construct the class
            ClassConstructMode mode = construct_mode;
            if (mode == ClassConstructMode::Auto)
            {
                mode = python_value_to_construct_mode(value);
            }

            JXC_DEBUG_ASSERT(mode != ClassConstructMode::Auto);
            switch (mode)
            {
            case ClassConstructMode::Value:
                return obj(value);
            case ClassConstructMode::ListAsArgs:
                return obj(*value);
            case ClassConstructMode::DictAsKeywordArgs:
                return obj(**value);
            default:
                out_error = ErrorInfo(jxc::format("Failed to construct Python type {}",
                    py::cast<std::string>(py::repr(obj))), ele.token.start_idx, ele.token.end_idx);
                break;
            }
        }
    }
    else
    {
        // treat the python object as a function
        return obj(value);
    }
    return value;
}


PyParser::PyParser(
    py::object data,
    ExpressionParseMode default_expr_parse_mode,
    bool ignore_unknown_annotations,
    bool ignore_unknown_number_suffixes)
    : buf(py::cast<std::string>(data))
    , parser(buf)
    , default_expr_parse_mode(default_expr_parse_mode)
    , ignore_unknown_annotations(ignore_unknown_annotations)
    , ignore_unknown_number_suffixes(ignore_unknown_number_suffixes)
{
}


std::optional<ValueConstructor> PyParser::get_construct_from_annotation(const Element& ele)
{
    if (ele.annotation.size() == 0)
    {
        return std::nullopt;
    }
    
    auto iter = annotation_type_map.find(ele.annotation);
    if (iter != annotation_type_map.end())
    {
        return iter->second;
    }

    if (find_construct_from_annotation != nullptr)
    {
        return find_construct_from_annotation(ele);
    }
    return std::nullopt;
}


std::optional<ValueConstructor> PyParser::get_construct_from_suffix(std::string_view suffix)
{
    if (suffix.size() == 0)
    {
        return std::nullopt;
    }

    auto iter = number_suffix_type_map.find(suffix);
    if (iter != number_suffix_type_map.end())
    {
        return iter->second;
    }

    if (find_construct_from_number_suffix != nullptr)
    {
        return find_construct_from_number_suffix(suffix);
    }
    return std::nullopt;
}


static Token make_token_from_pyobject(py::handle tok)
{
    if (tok.is_none())
    {
        return Token(TokenType::Null);
    }
    else if (py::isinstance<Token>(tok))
    {
        return py::cast<Token>(tok);
    }
    else if (py::isinstance<TokenType>(tok))
    {
        return Token(py::cast<TokenType>(tok));
    }
    else if (py::isinstance<py::str>(tok))
    {
        std::string_view tok_val = py::cast<std::string_view>(tok);
        const TokenType tok_type = token_type_from_symbol(tok_val);
        return token_type_has_value(tok_type)
            ? Token(tok_type, invalid_idx, invalid_idx, FlexString::make_owned(tok_val))
            : Token(tok_type);
    }
    else if (py::isinstance<py::tuple>(tok))
    {
        if (py::len(tok) != 2)
        {
            throw py::value_error(jxc::format("make_token_from_pyobject: tuple arg must be length-2 (got {})", py::len(tok)));
        }
        py::tuple vals = py::reinterpret_borrow<py::tuple>(tok);
        const std::string_view tok_val = py::cast<std::string_view>(vals[0]);
        const TokenType tok_type = py::cast<TokenType>(vals[1]);
        return token_type_has_value(tok_type)
            ? Token(tok_type, invalid_idx, invalid_idx, FlexString::make_owned(tok_val))
            : Token(tok_type);
    }
    throw py::type_error(jxc::format("Can't create token from {}", py::cast<std::string>(py::repr(tok))));
}


void PyParser::set_annotation_constructor(py::object annotation, py::object construct)
{
    OwnedTokenSpan anno;
    if (py::isinstance<py::tuple>(annotation) || py::isinstance<py::list>(annotation))
    {
        for (py::handle item : annotation)
        {
            anno.tokens.push(make_token_from_pyobject(item));
        }
    }
    else
    {
        anno.tokens.push(make_token_from_pyobject(annotation));
    }
    annotation_type_map.insert_or_assign(anno, ValueConstructor::from_python(construct));
}


void PyParser::set_number_suffix_constructor(py::str suffix, py::object construct)
{
    number_suffix_type_map.insert_or_assign(py::cast<std::string_view>(suffix), ValueConstructor::from_python(construct));
}


void PyParser::set_find_construct_from_annotation_callback(py::object callback)
{
    if (callback.is_none())
    {
        find_construct_from_annotation = nullptr;
    }
    else
    {
        find_construct_from_annotation = [this, callback](const Element& ele) -> std::optional<ValueConstructor>
        {
            JXC_DEBUG_ASSERT(ele.annotation.size() > 0);
            JXC_DEBUG_ASSERT(!annotation_type_map.contains(ele.annotation));
            auto result = ValueConstructor::from_python(callback(ele));
            annotation_type_map.insert({ OwnedTokenSpan(ele.annotation), result });
            return result;
        };
    }
}


void PyParser::set_find_construct_from_number_suffix_callback(py::object callback)
{
    if (callback.is_none())
    {
        find_construct_from_number_suffix = nullptr;
    }
    else
    {
        find_construct_from_number_suffix = [this, callback](std::string_view suffix) -> std::optional<ValueConstructor>
        {
            JXC_DEBUG_ASSERT(suffix.size() > 0);
            JXC_DEBUG_ASSERT(!number_suffix_type_map.contains(suffix));
            auto result = ValueConstructor::from_python(callback(suffix));
            number_suffix_type_map.insert({ std::string(suffix), result });
            return result;
        };
    }
}


void PyParser::set_custom_list_type(py::object new_type)
{
    if (new_type.is_none())
    {
        if (custom_list_type)
        {
            custom_list_type.reset();
        }
    }
    else
    {
        custom_list_type = py::type(new_type);
    }
}


void PyParser::set_custom_dict_type(py::object new_type)
{
    if (new_type.is_none())
    {
        if (custom_dict_type)
        {
            custom_dict_type.reset();
        }
    }
    else
    {
        custom_dict_type = py::type(new_type);
    }
}


bool PyParser::advance()
{
    if (parser.next())
    {
        return true;
    }
    else if (parser.get_error().is_err)
    {
        JXC_PYTHON_PARSE_ERROR(py::value_error, parser.get_error());
    }
    return false;
}


py::object PyParser::parse()
{
    if (advance())
    {
        return parse_value(parser.value());
    }
    return py::none();
}


py::object PyParser::parse_value(const Element& ele)
{
    py::object result;

    const bool have_annotation = ele.annotation.size() > 0;

    std::optional<ValueConstructor> type_constructor;
    if (have_annotation)
    {
        type_constructor = get_construct_from_annotation(ele);

        if (!ignore_unknown_annotations && !type_constructor.has_value())
        {
            JXC_UNKNOWN_ANNOTATION_ERROR(ele);
        }
    }

    switch (ele.type)
    {
    case ElementType::Null:
        result = py::none();
        break;
    case ElementType::Bool:
    {
        JXC_DEBUG_ASSERT(ele.token.type == TokenType::True || ele.token.type == TokenType::False);
        result = py::bool_((ele.token.type == TokenType::True) ? true : false);
        break;
    }
    case ElementType::Number:
        result = parse_number_element(ele);
        break;
    case ElementType::String:
        result = parse_string_element(ele);
        break;
    case ElementType::Bytes:
        result = parse_bytes_element(ele);
        break;
    case ElementType::DateTime:
        result = parse_datetime_element(ele);
        break;
    case ElementType::BeginArray:
        result = custom_list_type ? parse_list_custom() : parse_list();
        break;
    case ElementType::BeginExpression:
    {
        ExpressionParseMode parse_mode = default_expr_parse_mode;
        if (type_constructor && type_constructor->expr_parse_mode.has_value())
        {
            parse_mode = type_constructor->expr_parse_mode.value();
        }
        result = parse_expr(parse_mode);
        break;
    }
    case ElementType::BeginObject:
        result = custom_dict_type ? parse_dict_custom() : parse_dict();
        break;
    default:
        parse_error = ErrorInfo(jxc::format("Unexpected element type {}", element_type_to_string(ele.type)),
            ele.token.start_idx, ele.token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }

    if (type_constructor)
    {
        result = type_constructor->construct(result, ele, parse_error);
        if (parse_error.is_err)
        {
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
    }

    return result;
}


py::object PyParser::parse_number_element(const Element& ele)
{
    JXC_ASSERT(ele.token.type == TokenType::Number);

    util::NumberTokenSplitResult number;
    if (!util::split_number_token_value(ele.token, number, parse_error))
    {
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }

    const bool have_suffix = number.suffix.size() > 0;
    std::optional<ValueConstructor> suffix_construct;
    if (have_suffix)
    {
        suffix_construct = get_construct_from_suffix(number.suffix);

        if (!ignore_unknown_number_suffixes && !suffix_construct)
        {
            parse_error = ErrorInfo(jxc::format("Unknown number suffix {}", detail::debug_string_repr(number.suffix)), ele.token.start_idx, ele.token.end_idx);
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
    }

    if (number.is_integer())
    {
        int64_t int_value = 0;
        if (!util::parse_number<int64_t>(ele.token, int_value, number, parse_error))
        {
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }

        py::object result = py::int_(int_value);
        if (suffix_construct)
        {
            result = suffix_construct->construct(result, ele, parse_error);
            if (parse_error.is_err)
            {
                JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
            }
        }
        return result;
    }
    else
    {
        double float_value = 0.0;
        if (!util::parse_number<double>(ele.token, float_value, number, parse_error))
        {
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }

        py::object result = py::float_(float_value);
        if (suffix_construct)
        {
            result = suffix_construct->construct(result, ele, parse_error);
            if (parse_error.is_err)
            {
                JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
            }
        }
        return result;
    }
}


py::object PyParser::parse_string_element(const Element& ele)
{
    JXC_ASSERT(ele.token.type == TokenType::String);
    std::string result;
    if (!util::parse_string_token(ele.token, result, parse_error))
    {
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }

    try
    {
        return py::cast(result);
    }
    catch (const py::error_already_set& e)
    {
        parse_error = ErrorInfo(jxc::format("Failed creating Python string for `{}`: {}",
            detail::debug_string_repr(result), e.what()), ele.token.start_idx, ele.token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }
    catch (const py::cast_error& e)
    {
        parse_error = ErrorInfo(jxc::format("Failed casting to Python string: {}",
            e.what()), ele.token.start_idx, ele.token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }

    return py::none();
}


py::object PyParser::parse_bytes_element(const Element& ele)
{
    JXC_ASSERT(ele.token.type == TokenType::ByteString);

    py::bytearray result{};
    std::string_view value = ele.token.value.as_view();
    const size_t num_bytes = jxc::util::get_byte_buffer_required_size(value.data(), value.size());
    if (PyByteArray_Resize(result.ptr(), num_bytes) != 0)
    {
        JXC_DEBUG_ASSERT(PyErr_Occurred() != nullptr);
        throw py::error_already_set();
    }
    uint8_t* byte_buffer = reinterpret_cast<uint8_t*>(PyByteArray_AsString(result.ptr()));
    if (byte_buffer == nullptr)
    {
        JXC_DEBUG_ASSERT(PyErr_Occurred() != nullptr);
        throw py::error_already_set();
    }

    size_t num_bytes_written = 0;
    if (!util::parse_bytes_token(ele.token, byte_buffer, num_bytes, num_bytes_written, parse_error))
    {
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }

    return result;
}


py::object PyParser::parse_datetime_element(const Element& ele)
{
    JXC_ASSERT(ele.token.type == TokenType::DateTime);

    if (util::datetime_token_is_date(ele.token))
    {
        jxc::Date result;
        if (!util::parse_date_token(ele.token, result, parse_error))
        {
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
        return py::cast(result);
    }
    else
    {
        jxc::DateTime result;
        if (!util::parse_datetime_token(ele.token, result, parse_error))
        {
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
        return py::cast(result);
    }
}


py::object PyParser::parse_list()
{
    const Element& start_ele = parser.value();

    py::list result{};
    JXC_ASSERT(start_ele.type == ElementType::BeginArray);
    while (advance())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndArray)
        {
            break;
        }
        result.append(parse_value(ele));
    }

    return result;
}


py::object PyParser::parse_list_custom()
{
    JXC_DEBUG_ASSERT(custom_list_type.has_value());

    const Element& start_ele = parser.value();

    py::object result = (*custom_list_type)();
    JXC_ASSERT(start_ele.type == ElementType::BeginArray);
    while (advance())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndArray)
        {
            break;
        }
        result.attr("append")(parse_value(ele));
    }

    return result;
}


py::object PyParser::parse_expr_value(const Element& ele)
{
    switch (ele.type)
    {
    case ElementType::Null:
        return py::none();
    case ElementType::Bool:
        return (ele.token.type == TokenType::True) ? py::bool_(true) : py::bool_(false);
    case ElementType::Number:
        return parse_number_element(ele);
    case ElementType::String:
        return parse_string_element(ele);
    case ElementType::Bytes:
        return parse_bytes_element(ele);
    case ElementType::DateTime:
        return parse_datetime_element(ele);
    case ElementType::ExpressionToken:
        return py::cast(ele.token.value);
    case ElementType::Comment:
        return py::cast(ele.token.value);
    default:
        break;
    }

    parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
        ele.token.start_idx, ele.token.end_idx);
    JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
}


py::object PyParser::parse_expr_token(const Element& ele)
{
    if (element_is_expression_value_type(ele.type))
    {
        return py::cast(ele.token.copy(), py::return_value_policy::move);
    }

    parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
        ele.token.start_idx, ele.token.end_idx);
    JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
}


py::object PyParser::parse_expr(ExpressionParseMode parse_mode)
{
    const Element& start_ele = parser.value();
    const size_t expr_start_idx = start_ele.token.start_idx;
    JXC_ASSERT(start_ele.type == ElementType::BeginExpression);

    py::object result;

    switch (parse_mode)
    {
    case ExpressionParseMode::ValueList:
    {
        py::list list_result{};
        while (advance())
        {
            const Element& ele = parser.value();
            if (ele.type == ElementType::EndExpression)
            {
                break;
            }
            else if (ele.type != ElementType::Comment)
            {
                list_result.append(parse_expr_value(ele));
            }
        }
        result = list_result;
        break;
    }
    case ExpressionParseMode::ValueListWithComments:
    {
        py::list list_result{};
        while (advance())
        {
            const Element& ele = parser.value();
            if (ele.type == ElementType::EndExpression)
            {
                break;
            }
            list_result.append(parse_expr_value(ele));
        }
        result = list_result;
        break;
    }
    case ExpressionParseMode::TokenList:
    {
        py::list list_result{};
        while (advance())
        {
            const Element& ele = parser.value();
            if (ele.type == ElementType::EndExpression)
            {
                break;
            }
            list_result.append(parse_expr_token(ele));
        }
        result = list_result;
        break;
    }
    case ExpressionParseMode::SourceString:
    {
        size_t expr_inner_start_idx = invalid_idx;
        size_t expr_inner_end_idx = invalid_idx;
        bool inside_expr = true;
        while (inside_expr && advance())
        {
            const Element& ele = parser.value();
            if (expr_inner_start_idx == invalid_idx)
            {
                expr_inner_start_idx = ele.token.start_idx;
            }

            if (element_is_expression_value_type(ele.type))
            {
                expr_inner_end_idx = ele.token.end_idx;
            }
            else if (ele.type == ElementType::EndExpression)
            {
                inside_expr = false;
            }
            else
            {
                parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
                    ele.token.start_idx, ele.token.end_idx);
                JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
            }
        }

        if (expr_inner_start_idx != invalid_idx && expr_inner_end_idx != invalid_idx && expr_inner_end_idx >= expr_inner_start_idx)
        {
            std::string_view expr_inner_view = std::string_view(buf).substr(expr_inner_start_idx, expr_inner_end_idx - expr_inner_start_idx);
            result = py::str(expr_inner_view.data(), expr_inner_view.size());
        }
        else
        {
            parse_error = ErrorInfo("Failed to get valid source string span for expression",
                expr_start_idx, parser.value().token.end_idx);
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
        break;
    }
    default:
        parse_error = ErrorInfo(jxc::format("Invalid ExpressionParseMode value {}",
            static_cast<int32_t>(parse_mode)), expr_start_idx, parser.value().token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        break;
    }

    return result;
}


py::object PyParser::parse_key(const Element& ele)
{
    JXC_ASSERT(ele.type == ElementType::ObjectKey);
    switch (ele.token.type)
    {
    case TokenType::Identifier:
        return py::cast(ele.token.value);
    case TokenType::True:
        return py::bool_(true);
    case TokenType::False:
        return py::bool_(false);
    case TokenType::Null:
        return py::none();
    case TokenType::Number:
    {
        py::object number_val = parse_number_element(ele);
        if (py::isinstance<py::float_>(number_val))
        {
            parse_error = ErrorInfo("Got float type for dict key", ele.token.start_idx, ele.token.end_idx);
            JXC_PYTHON_PARSE_ERROR(py::type_error, parse_error);
        }
        return number_val;
    }
    case TokenType::String:
        return parse_string_element(ele);
    case TokenType::ByteString:
        return parse_bytes_element(ele);
    default:
        parse_error = ErrorInfo(jxc::format("Invalid token for dict key {}", ele.token.to_repr()),
            ele.token.start_idx, ele.token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }
}


py::object PyParser::parse_dict()
{
    const Element& start_ele = parser.value();
    JXC_ASSERT(start_ele.type == ElementType::BeginObject);

    py::dict result{};
    while (advance())
    {
        const Element& key_ele = parser.value();
        if (key_ele.type == ElementType::EndObject)
        {
            break;
        }
        py::object key = parse_key(key_ele);
        if (!advance())
        {
            break;
        }
        result[key] = parse_value(parser.value());
    }

    return result;
}


py::object PyParser::parse_dict_custom()
{
    JXC_DEBUG_ASSERT(custom_dict_type.has_value());

    const Element& start_ele = parser.value();
    JXC_ASSERT(start_ele.type == ElementType::BeginObject);

    py::object result = (*custom_dict_type)();
    while (advance())
    {
        const Element& key_ele = parser.value();
        if (key_ele.type == ElementType::EndObject)
        {
            break;
        }
        py::object key = parse_key(key_ele);
        if (!advance())
        {
            break;
        }

        result[key] = parse_value(parser.value());
    }

    return result;
}

} // namespace jxc
