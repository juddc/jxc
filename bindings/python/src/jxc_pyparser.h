#pragma once
#include "jxc/jxc.h"
#include "jxc/jxc_util.h"
#include "jxc/jxc_parser.h"
#include "jxc_python_util.h"
#include <unordered_map>

#include "jxc_python.h"


namespace jxc
{


// Wrapper around lexer that keeps a reference to the Python source buffer
// so it doesn't get GC'd while using it.
class JXC_NOEXPORT PyLexer
{
private:
    py::str buf;
    std::string_view buf_view;
    Lexer lexer;
    ErrorInfo lex_error;

public:
    PyLexer(py::str buf)
        : buf(buf)
        , buf_view(py::cast<std::string_view>(buf))
        , lexer(buf_view.data(), buf_view.size())
    {
    }

    Token next()
    {
        Token result{};
        lexer.next(result, lex_error);
        return result;
    }

    const ErrorInfo& get_error() const
    {
        return lex_error;
    }

    py::tuple get_token_pos()
    {
        size_t start = 0, end = 0;
        lexer.get_token_pos(start, end);
        return py::make_tuple(start, end);
    }
};


// Annotation-specific lexer
class JXC_NOEXPORT PyAnnotationLexer
{
private:
    py::str buf;
    std::string_view buf_view;
    AnnotationLexer lexer;

public:
    PyAnnotationLexer(py::str buf)
        : buf(buf)
        , buf_view(py::cast<std::string_view>(buf))
        , lexer(buf_view)
    {
    }

    inline Token next()
    {
        Token result{};
        lexer.next(result);
        return result;
    }

    inline bool has_error() const
    {
        return lexer.has_error();
    }

    inline py::str get_error_message() const
    {
        return (lexer.get_error_message().size() > 0) ? py::cast(lexer.get_error_message()) : py::str("");
    }
};


// see comments in the enum binding for this in jxc_python.cpp
enum class ExpressionParseMode : uint8_t
{
    ValueList = 0,
    ValueAndTokenList,
    TokenList,
    SourceString,
};


// see comments in the enum binding for this in jxc_python.cpp
enum class ClassConstructMode : uint8_t
{
    Auto = 0,
    Value,
    ListAsArgs,
    DictAsKeywordArgs,
};


struct JXC_NOEXPORT ValueConstructor
{
    py::object obj;
    bool is_type = false;
    bool has_inline_decode_method = false;
    ClassConstructMode construct_mode = ClassConstructMode::Auto;
    std::optional<ExpressionParseMode> expr_parse_mode;

    ValueConstructor() {}

    static std::optional<ValueConstructor> from_python(py::object value);

    py::object construct(py::object value, const Element& ele, ErrorInfo& out_error) const;
};


inline ClassConstructMode python_value_to_construct_mode(py::object val)
{
    if (py::isinstance<py::sequence>(val) || py::isinstance<py::list>(val) || py::isinstance<py::tuple>(val))
    {
        return ClassConstructMode::ListAsArgs;
    }
    else if (py::isinstance<py::dict>(val))
    {
        return ClassConstructMode::DictAsKeywordArgs;
    }
    else
    {
        return ClassConstructMode::Value;
    }
}


// converts a Python value to a Token
Token make_token_from_python_value(py::handle tok);

// converts a Python value to a TokenList, interpreting each value as a token or sequence of tokens
TokenList make_token_list_from_python_values(py::args tokens);


class JXC_NOEXPORT PyParser
{
public:
    using OverrideConstructFunc = std::function<py::object(const Element&)>;
    using FindConstructAnnotationFunc = std::function<std::optional<ValueConstructor>(const Element&)>;
    using FindConstructNumberSuffixFunc = std::function<std::optional<ValueConstructor>(std::string_view)>;

private:
    std::string buf;
    JumpParser parser;
    ErrorInfo parse_error;
    bool ignore_unknown_annotations = true;
    bool ignore_unknown_number_suffixes = true;
    ExpressionParseMode default_expr_parse_mode = ExpressionParseMode::ValueList;
    std::optional<py::type> custom_list_type;
    std::optional<std::string> custom_list_type_append_func_name;
    std::optional<py::type> custom_dict_type;

    FindConstructAnnotationFunc find_construct_from_annotation;
    FindConstructNumberSuffixFunc find_construct_from_number_suffix;

    python_util::AnnotationMap<std::optional<ValueConstructor>> annotation_type_map;
    python_util::StringMap<std::optional<ValueConstructor>> number_suffix_type_map;
    ankerl::unordered_dense::map<ElementType, OverrideConstructFunc> element_parse_override_map;

    std::optional<ValueConstructor> get_construct_from_annotation(const Element& ele);
    std::optional<ValueConstructor> get_construct_from_suffix(std::string_view suffix);

public:
    PyParser() = default;

    PyParser(py::object buf,
        ExpressionParseMode default_expr_parse_mode,
        bool ignore_unknown_annotations,
        bool ignore_unknown_number_suffixes);

    void reset(py::object new_buf)
    {
        buf = py::cast<std::string>(new_buf);
        parser.reset(buf);
        parse_error.reset();
    }

    ExpressionParseMode get_default_expr_parse_mode() const { return default_expr_parse_mode; }
    void set_default_expr_parse_mode(ExpressionParseMode new_value) { default_expr_parse_mode = new_value; }

    bool get_ignore_unknown_annotations() const { return ignore_unknown_annotations; }
    void set_ignore_unknown_annotations(bool new_value) { ignore_unknown_annotations = new_value; }

    bool get_ignore_unknown_number_suffixes() const { return ignore_unknown_number_suffixes; }
    void set_ignore_unknown_number_suffixes(bool new_value) { ignore_unknown_number_suffixes = new_value; }

    void set_override_element_parse_function(ElementType element_type, py::object parse_callback);

    void set_annotation_constructor(py::object annotation, py::object construct);
    void set_number_suffix_constructor(py::str suffix, py::object construct);
    void set_find_construct_from_annotation_callback(py::object callback);
    void set_find_construct_from_number_suffix_callback(py::object callback);

    void set_custom_list_type(py::object new_type, py::str append_func_name);
    void set_custom_dict_type(py::object new_type);

    bool has_error() const { return parse_error.is_err || parser.has_error(); }
    const ErrorInfo& get_error() const { return parse_error.is_err ? parse_error : parser.get_error(); }

    bool advance();

    inline const Element& current_element() const { return parser.value(); }
    inline py::object current_element_py() const { return detail::cast_element_to_python(parser.value()); }

    py::object parse();
    py::object parse_value();
    py::object parse_number();
    py::object parse_string();
    py::object parse_bytes();
    py::object parse_datetime();
    py::object parse_list();
    py::object parse_list_custom(py::type list_type, const std::string& append_func_name);
    py::object parse_expr_value(bool allow_tokens);
    Token parse_expr_token();
    py::object parse_expr(ExpressionParseMode parse_mode);
    py::object parse_dict_key();
    py::object parse_dict();
    py::object parse_dict_custom(py::type dict_type);
};

} // namespace jxc
