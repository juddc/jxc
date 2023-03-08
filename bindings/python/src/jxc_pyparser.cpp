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


#define JXC_REQUIRE_ELEMENT_TYPE(ELE, REQ_ELE_TYPE) do { \
    if ((ELE).type != ElementType::REQ_ELE_TYPE) { \
        parse_error = ErrorInfo(jxc::format("Expected " #REQ_ELE_TYPE " element, got {}", element_type_to_string((ELE).type)), \
            (ELE).token.start_idx, (ELE).token.end_idx); \
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error); \
    } \
} while(0)


#define JXC_REQUIRE_TOKEN_TYPE(TOK, REQ_TOK_TYPE) do { \
    if ((TOK).type != TokenType::REQ_TOK_TYPE) { \
        parse_error = ErrorInfo(jxc::format("Expected " #REQ_TOK_TYPE " token, got {}", token_type_to_string((TOK).type)), \
            (TOK).start_idx, (TOK).end_idx); \
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error); \
    } \
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


Token make_token_from_python_value(py::handle tok)
{
    if (tok.is_none())
    {
        return Token(TokenType::Null);
    }
    else if (py::isinstance<py::str>(tok))
    {
        std::string_view str_value = py::cast<std::string_view>(tok);

        if (str_value.size() == 0)
        {
            throw py::value_error("Can't convert string \"\" to token");
        }

        // See if we can determine the token type from the string.
        // eg. '+' == TokenType::Plus
        const TokenType tok_type = token_type_from_symbol(str_value, false);
        if (tok_type != TokenType::Invalid)
        {
            return Token(tok_type, invalid_idx, invalid_idx, FlexString(str_value));
        }
        else
        {
            // try using the lexer to identify the token, since token_type_from_symbol is built to be fast, and doesn't handle all cases
            TokenLexer lexer(str_value);
            size_t tok_count = 0;
            Token result;

            Token tok;
            while (lexer.next(tok))
            {
                if (tok_count == 0)
                {
                    result = tok.copy();
                }
                else
                {
                    throw py::value_error(jxc::format("String {} is more than one token",
                        detail::debug_string_repr(str_value)));
                }

                ++tok_count;
            }

            if (lexer.has_error())
            {
                throw py::value_error(jxc::format("String {} is not a valid token: {}",
                    detail::debug_string_repr(str_value), lexer.get_error_message()));
            }

            return result;
        }
        
        throw py::value_error(jxc::format("Can't convert string {} to token", detail::debug_string_repr(str_value)));
    }
    else if (py::isinstance<TokenType>(tok))
    {
        return Token(py::cast<TokenType>(tok));
    }
    else if (py::isinstance<py::tuple>(tok))
    {
        // accept tuples in the form (Value, TokenType)
        py::tuple tup_val = py::reinterpret_borrow<py::tuple>(tok);
        if (tup_val.size() == 2)
        {
            const TokenType tok_type = py::cast<TokenType>(tup_val[1]);
            return token_type_has_value(tok_type)
                ? Token(tok_type, invalid_idx, invalid_idx, FlexString::make_owned(py::cast<std::string_view>(tup_val[0])))
                : Token(tok_type);
        }
        else
        {
            throw py::type_error(jxc::format("Expected 2-tuple in the form (value, token_type), got {}", py::cast<std::string>(py::repr(tok))));
        }
    }
    else if (py::isinstance<Token>(tok))
    {
        return py::cast<Token>(tok);
    }
    else
    {
        throw py::type_error(jxc::format("Failed to convert value {} to Token", py::cast<std::string>(py::repr(tok))));
    }
}


TokenList make_token_list_from_python_values(py::args tokens)
{
    TokenList result;

    for (py::handle item : tokens)
    {
        if (py::isinstance<py::str>(item))
        {
            // Handle string values ourselves here instead of relying on make_token_from_python_value, because strings can potentially
            // contain multiple tokens, and make_token_from_python_value can't return more than one.
            std::string_view str_value = py::cast<std::string_view>(item);
            if (str_value.size() == 0)
            {
                continue;
            }

            // See if we can determine the token type from the string.
            // eg. '+' == TokenType::Plus
            const TokenType tok_type = token_type_from_symbol(str_value, false);
            if (tok_type != TokenType::Invalid)
            {
                result.tokens.push(Token(tok_type, invalid_idx, invalid_idx, FlexString(str_value)));
            }
            else
            {
                // treat the string as a sequence of tokens - lex them and push them all to the output
                TokenLexer lexer(str_value);
                Token tok;
                while (lexer.next(tok))
                {
                    result.tokens.push(tok.copy());
                }
                if (lexer.has_error())
                {
                    throw py::value_error(jxc::format("Lexer error converting string into tokens: {}", lexer.get_error_message()));
                }
            }
        }
        else
        {
            result.tokens.push(make_token_from_python_value(item));
        }
    }

    return result;
}


static TokenList make_annotation_token_list(py::handle annotation)
{
    if (py::isinstance<py::tuple>(annotation) || py::isinstance<py::list>(annotation))
    {
        return make_token_list_from_python_values(py::cast<py::tuple>(annotation));
    }
    else if (annotation.is_none()
        || py::isinstance<py::str>(annotation)
        || py::isinstance<TokenType>(annotation)
        || py::isinstance<Token>(annotation))
    {
        return make_token_list_from_python_values(py::make_tuple(annotation));
    }
    else if (py::isinstance<TokenList>(annotation))
    {
        return py::cast<TokenList>(annotation);
    }

    // assume single token
    TokenList result;
    result.tokens.push(make_token_from_python_value(annotation));
    return result;
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


void PyParser::set_override_element_parse_function(ElementType element_type, py::object parse_callback)
{
    if (parse_callback.is_none())
    {
        if (element_parse_override_map.contains(element_type))
        {
            element_parse_override_map.erase(element_type);
        }
    }
    else
    {
        element_parse_override_map.insert_or_assign(element_type, [parse_callback](const Element& ele) -> py::object
        {
            return parse_callback(detail::cast_element_to_python(ele));
        });
    }
}


void PyParser::set_annotation_constructor(py::object annotation, py::object construct)
{
    TokenList anno = make_annotation_token_list(annotation);
    if (anno.size() == 0)
    {
        throw py::value_error(jxc::format("Value {} evaluated to empty annotation",
            detail::debug_string_repr(py::cast<std::string>(py::repr(annotation)))));
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

            TokenList anno_list(ele.annotation);

            auto result = ValueConstructor::from_python(callback(py::cast(anno_list, py::return_value_policy::copy)));

            // cache the constructor callback so we don't have to do this lookup for every duplicate suffix
            annotation_type_map.insert({ anno_list, result });

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

            // cache the constructor callback so we don't have to do this lookup for every duplicate suffix
            number_suffix_type_map.insert({ std::string(suffix), result });

            return result;
        };
    }
}


void PyParser::set_custom_list_type(py::object new_type, py::str append_func_name)
{
    if (new_type.is_none())
    {
        if (custom_list_type)
        {
            custom_list_type.reset();
        }

        if (custom_list_type_append_func_name)
        {
            custom_list_type_append_func_name.reset();
        }
    }
    else
    {
        custom_list_type = py::type(new_type);
        custom_list_type_append_func_name = py::cast<std::string>(append_func_name);
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
        return parse_value();
    }
    return py::none();
}


py::object PyParser::parse_value()
{
    py::object result;

    const Element& ele = parser.value();

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

    if (element_parse_override_map.size() > 0 && element_parse_override_map.contains(ele.type))
    {
        result = element_parse_override_map[ele.type](ele);
    }
    else
    {
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
            result = parse_number();
            break;
        case ElementType::String:
            result = parse_string();
            break;
        case ElementType::Bytes:
            result = parse_bytes();
            break;
        case ElementType::DateTime:
            result = parse_datetime();
            break;
        case ElementType::BeginArray:
            if (custom_list_type)
            {
                result = parse_list_custom(*custom_list_type, custom_list_type_append_func_name.value_or(std::string()));
            }
            else
            {
                result = parse_list();
            }
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
            result = custom_dict_type ? parse_dict_custom(*custom_dict_type) : parse_dict();
            break;
        default:
            parse_error = ErrorInfo(jxc::format("Unexpected element type {}", element_type_to_string(ele.type)),
                ele.token.start_idx, ele.token.end_idx);
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
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


static py::object python_int_from_string(std::string_view str, int base = 10, int32_t multiplier = 1)
{
    char* pend = nullptr;
    PyObject* val = PyLong_FromString(str.data(), &pend, base);
    if (PyErr_Occurred() != nullptr)
    {
        throw py::error_already_set();
    }
    JXC_ASSERT(pend != nullptr && pend == str.data() + str.size());
    if (multiplier == 1)
    {
        return py::reinterpret_steal<py::int_>(val);
    }
    return py::reinterpret_steal<py::int_>(val) * py::int_(multiplier);
}


// Constructs a Python int directly from a string.
// This allows creating integers larger than int64_t and uint64_t can handle because of Python's variable-width integers.
static py::object parse_python_large_integer(const util::NumberTokenSplitResult& number)
{
    JXC_DEBUG_ASSERT(number.is_integer());
    py::object result;

    int32_t multiplier = 1;
    if (number.sign == '-')
    {
        multiplier *= -1;
    }

    // number.is_integer() should check for this
    JXC_DEBUG_ASSERT(number.exponent >= 0);

    if (number.exponent > 0)
    {
        int32_t exp = number.exponent;
        while (exp > 0)
        {
            multiplier *= 10;
            --exp;
        }
    }

    if (number.prefix.size() == 2)
    {
        switch (number.prefix[1])
        {
        case 'x':
        case 'X':
            result = python_int_from_string(number.value, 16, multiplier);
            break;
        case 'b':
        case 'B':
            result = python_int_from_string(number.value, 2, multiplier);
            break;
        case 'o':
        case 'O':
            result = python_int_from_string(number.value, 8, multiplier);
            break;
        default:
            throw py::value_error(jxc::format("Invalid numeric prefix {}", jxc::detail::debug_string_repr(number.prefix)));
        }
    }
    else
    {
        result = python_int_from_string(number.value, 10, multiplier);
    }

    return result;
}


py::object PyParser::parse_number()
{
    const Element& ele = parser.value();
    JXC_REQUIRE_TOKEN_TYPE(ele.token, Number);

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

    py::object result;

    if (number.is_integer())
    {
        if (number.value.size() <= 18)
        {
            // INT64_MAX is 19 digits, so 1-18 digits should always be safe for an int64_t conversion without errors
            int64_t int_value = 0;
            if (!util::parse_number<int64_t>(ele.token, int_value, number, parse_error))
            {
                JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
            }

            result = py::int_(int_value);
        }
        else
        {
            result = parse_python_large_integer(number);
        }
    }
    else
    {
        double result_float = 0.0;
        if (!util::parse_number<double>(ele.token, result_float, number, parse_error))
        {
            JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
        }
        result = py::float_(result_float);
    }

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


py::object PyParser::parse_string()
{
    const Element& ele = parser.value();
    JXC_REQUIRE_TOKEN_TYPE(ele.token, String);
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


py::object PyParser::parse_bytes()
{
    const Element& ele = parser.value();
    JXC_REQUIRE_TOKEN_TYPE(ele.token, ByteString);

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


py::object PyParser::parse_datetime()
{
    const Element& ele = parser.value();
    JXC_REQUIRE_TOKEN_TYPE(ele.token, DateTime);
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
    py::list result{};
    JXC_REQUIRE_ELEMENT_TYPE(parser.value(), BeginArray);
    while (advance())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndArray)
        {
            break;
        }
        result.append(parse_value());
    }

    return result;
}


py::object PyParser::parse_list_custom(py::type list_type, const std::string& append_func_name)
{
    JXC_DEBUG_ASSERT(list_type.ptr() != nullptr);
    py::object result = list_type();

    const char* append = (append_func_name.size() > 0) ? append_func_name.c_str() : "append";

    JXC_REQUIRE_ELEMENT_TYPE(parser.value(), BeginArray);
    while (advance())
    {
        const Element& ele = parser.value();
        if (ele.type == ElementType::EndArray)
        {
            break;
        }
        result.attr(append)(parse_value());
    }
    return result;
}


py::object PyParser::parse_expr_value(bool allow_tokens)
{
    const Element& ele = parser.value();
    switch (ele.type)
    {
    case ElementType::Null:
        return py::none();
    case ElementType::Bool:
        return (ele.token.type == TokenType::True) ? py::bool_(true) : py::bool_(false);
    case ElementType::Number:
        return parse_number();
    case ElementType::String:
        return parse_string();
    case ElementType::Bytes:
        return parse_bytes();
    case ElementType::DateTime:
        return parse_datetime();
    case ElementType::Comment:
        // fallthrough
    case ElementType::ExpressionToken:
        if (allow_tokens)
        {
            return py::cast(std::move(ele.token.copy()), py::return_value_policy::move);
        }
        else
        {
            return py::cast(ele.token.value);
        }
    default:
        break;
    }

    parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
        ele.token.start_idx, ele.token.end_idx);
    JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
}


Token PyParser::parse_expr_token()
{
    const Element& ele = parser.value();
    if (!element_is_expression_value_type(ele.type))
    {
        parse_error = ErrorInfo(jxc::format("Invalid element for expression value {}", element_type_to_string(ele.type)),
            ele.token.start_idx, ele.token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }

    return ele.token.copy();
}


py::object PyParser::parse_expr(ExpressionParseMode parse_mode)
{
    py::object result;
    JXC_REQUIRE_ELEMENT_TYPE(parser.value(), BeginExpression);
    const size_t expr_start_idx = parser.value().token.start_idx;
    bool allow_tokens = false;

    switch (parse_mode)
    {
    case ExpressionParseMode::ValueAndTokenList:
        allow_tokens = true;
        // fallthrough
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
            else if (ele.type == ElementType::Comment)
            {
                continue;
            }

            list_result.append(parse_expr_value(allow_tokens));
        }
        result = list_result;
        break;
    }
    case ExpressionParseMode::TokenList:
    {
        TokenList list_result;
        while (advance())
        {
            const Element& ele = parser.value();
            if (ele.type == ElementType::EndExpression)
            {
                break;
            }
            list_result.tokens.push(parse_expr_token());
        }

        // get the source substring for the token list
        if (list_result.size() > 0)
        {
            const size_t start_idx = list_result[0].start_idx;
            const size_t end_idx = list_result[list_result.size() - 1].end_idx;
            if (end_idx >= start_idx && start_idx < buf.size() && end_idx < buf.size())
            {
                const size_t source_len = end_idx - start_idx;
                list_result.src = FlexString(std::string_view(buf).substr(start_idx, source_len));
            }
        }

        result = py::cast(list_result);
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


py::object PyParser::parse_dict_key()
{
    const Element& ele = parser.value();
    JXC_REQUIRE_ELEMENT_TYPE(ele, ObjectKey);
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
        py::object number_val = parse_number();
        if (py::isinstance<py::float_>(number_val))
        {
            parse_error = ErrorInfo("Got float type for dict key", ele.token.start_idx, ele.token.end_idx);
            JXC_PYTHON_PARSE_ERROR(py::type_error, parse_error);
        }
        return number_val;
    }
    case TokenType::String:
        return parse_string();
    case TokenType::ByteString:
        return parse_bytes();
    default:
        parse_error = ErrorInfo(jxc::format("Invalid token for dict key {}", ele.token.to_repr()),
            ele.token.start_idx, ele.token.end_idx);
        JXC_PYTHON_PARSE_ERROR(py::value_error, parse_error);
    }
}


py::object PyParser::parse_dict()
{
    const Element& start_ele = parser.value();
    JXC_REQUIRE_ELEMENT_TYPE(start_ele, BeginObject);
    py::dict result{};
    while (advance())
    {
        const Element& key_ele = parser.value();
        if (key_ele.type == ElementType::EndObject)
        {
            break;
        }
        py::object key = parse_dict_key();
        if (!advance())
        {
            break;
        }
        result[key] = parse_value();
    }

    return result;
}


py::object PyParser::parse_dict_custom(py::type dict_type)
{
    JXC_DEBUG_ASSERT(dict_type.ptr() != nullptr);
    const Element& start_ele = parser.value();
    JXC_REQUIRE_ELEMENT_TYPE(start_ele, BeginObject);
    py::object result = dict_type();
    while (advance())
    {
        const Element& key_ele = parser.value();
        if (key_ele.type == ElementType::EndObject)
        {
            break;
        }
        py::object key = parse_dict_key();
        if (!advance())
        {
            break;
        }

        result[key] = parse_value();
    }
    return result;
}

} // namespace jxc
