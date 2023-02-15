#include "jxc/jxc.h"

#include "jxc_python.h"
#include "jxc_pyparser.h"
#include "jxc_pyserializer.h"


JXC_BEGIN_NAMESPACE(jxc)

JXC_BEGIN_NAMESPACE(detail)

bool is_python_date(py::handle value)
{
    // lazy initialization for the PyDateTime API
    if (!PyDateTimeAPI)
    {
        PyDateTime_IMPORT;
    }

    return value && PyDate_Check(value.ptr());
}

bool is_python_datetime(py::handle value)
{
    // lazy initialization for the PyDateTime API
    if (!PyDateTimeAPI)
    {
        PyDateTime_IMPORT;
    }

    return value && PyDateTime_Check(value.ptr());
}

JXC_END_NAMESPACE(detail)

template<typename T>
auto bind_tokenspan(py::module_& m, const char* name)
{
    return py::class_<T>(m, name)
        .def("__bool__", &T::operator bool)
        .def("__getitem__", [](const T& self, int64_t idx) -> const Token& { return self[detail::python_index(idx, self.size())]; })
        .def("__eq__", [](const T& self, const T& rhs) { return self == rhs; })
        .def("__ne__", [](const T& self, const T& rhs) { return self != rhs; })
        .def("__len__", &T::size)
        .def("__repr__", &T::to_repr)
        .def("__str__", &T::to_string)
        .def("__hash__", &T::hash)
        .def("copy", [](const T& self) -> OwnedTokenSpan { return OwnedTokenSpan(self); })
        .def("source", [](const T& self) { return self.source(false); },
            "The original text from the buffer that represents all the tokens in this span. "
            "Primarily useful for accessing the original whitespace.");
}


template<typename T>
auto bind_element(py::module_& m, const char* name)
{
    using anno_type = typename T::anno_type;

    auto cls = py::class_<T>(m, name);

    cls.def(py::init([](ElementType element_type, const Token& token, const anno_type& annotation) -> T
    {
        return T{ element_type, token, annotation };
    }),
        py::arg("element_type"),
        py::arg("token"),
        py::arg("annotation"));

    if constexpr (std::is_same_v<T, OwnedTokenSpan>)
    {
        cls.def(py::init([](ElementType element_type, const Token& token, py::sequence annotation) -> T
        {
            OwnedTokenSpan anno_tokens;
            for (const auto& tok : annotation)
            {
                if (py::isinstance<TokenType>(tok))
                {
                    anno_tokens.tokens.push(Token(py::cast<TokenType>(tok)));
                }
                else if (py::isinstance<Token>(tok))
                {
                    anno_tokens.tokens.push(Token(py::cast<Token>(tok)));
                }
                else
                {
                    throw py::type_error(jxc::format("Expected TokenType or Token, got {}", py::cast<std::string>(py::repr(tok))));
                }
            }
            return T{ element_type, token, anno_tokens };
        }),
            py::arg("element_type"),
            py::arg("token"),
            py::arg("annotation") = py::make_tuple());
    }

    cls.def_readwrite("type", &T::type);
    cls.def_property("token",
        [](const T& self) { return self.token; },
        [](T& self, const Token& new_token) { self.token = new_token; self.token.to_owned_inplace(); }
    );
    cls.def_readonly("annotation", &T::annotation);

    auto element_eq = [](const T& self, py::object rhs) -> bool
    {
        if (rhs.is_none())
        {
            return false;
        }
        else if (py::isinstance<T>(rhs))
        {
            return self == py::cast<T>(rhs);
        }

        if constexpr (std::is_same_v<T, Element>)
        {
            // compare against an owned element
            if (py::isinstance<OwnedElement>(rhs))
            {
                return self == py::cast<OwnedElement>(rhs);
            }
        }
        else if constexpr (std::is_same_v<T, OwnedElement>)
        {
            // compare against a non-owned element
            if (py::isinstance<Element>(rhs))
            {
                return self == py::cast<Element>(rhs);
            }
        }

        return false;
    };

    cls.def("__eq__", [element_eq](const T& self, py::object rhs) -> bool { return element_eq(self, rhs); });
    cls.def("__ne__", [element_eq](const T& self, py::object rhs) -> bool { return !element_eq(self, rhs); });

    cls.def("__str__", &T::to_string);
    cls.def("__repr__", &T::to_repr);

    return cls;
}

JXC_END_NAMESPACE(jxc)



uint32_t python_value_to_codepoint(py::object value)
{
    if (py::isinstance<py::str>(value))
    {
        py::str str = value;
        const size_t str_len = py::len(str);
        if (str_len != 1)
        {
            throw py::value_error(jxc::format("Expected string of length 1, got length {}", str_len));
        }
        py::object builtins_mod = py::module_::import("builtins");
        py::object ord_fn = builtins_mod.attr("ord");
        return py::cast<uint32_t>(ord_fn(str));
    }
    else if (py::isinstance<py::int_>(value))
    {
        return py::cast<uint32_t>(value);
    }
    else
    {
        throw py::type_error(jxc::format("Expected str or int, got {}", py::cast<std::string>(py::repr(value))));
    }
}



PYBIND11_MODULE(_pyjxc, m)
{
    using namespace jxc;

    m.attr("__version__") = jxc::format("{}.{}.{}", JXC_VERSION_MAJOR, JXC_VERSION_MINOR, JXC_VERSION_PATCH);
    m.attr("invalid_idx") = invalid_idx;

    m.def("set_custom_assert_handler", [](py::object handler)
    {
        set_custom_assert_handler([handler](std::string_view file, int line, std::string_view cond, std::string&& msg)
        {
            try
            {
                handler(file, line, cond, msg);
            }
            catch (const py::cast_error& e)
            {
                jxc::print("Got Python cast error in assert handler: {}\n", e.what());
            }
        });
    });

    py::enum_<LogLevel>(m, "LogLevel")
        .value("Info", LogLevel::Info)
        .value("Warning", LogLevel::Warning)
        .value("Error", LogLevel::Error)
        .value("Fatal", LogLevel::Fatal)
    ;

    m.def("set_custom_log_handler", [](py::object handler)
    {
        if (handler.is_none())
        {
            clear_custom_log_handler();
        }
        else
        {
            set_custom_log_handler([handler](LogLevel level, std::string&& msg)
            {
                handler(py::cast(level), py::cast(std::move(msg)));
            });
        }
    });

    // this is mostly just useful for testing custom assert handlers
    m.def("_jxc_assert", [](bool cond, const std::string& msg) { JXC_ASSERTF(cond, "{}", msg); });

    py::enum_<FloatLiteralType>(m, "FloatLiteralType")
        .value("Finite", FloatLiteralType::Finite, "Normal floating point value that is not nan, inf, or -inf")
        .value("NotANumber", FloatLiteralType::NotANumber, "Equivalent to float('nan')")
        .value("PosInfinity", FloatLiteralType::PosInfinity, "Equivalent to float('inf')")
        .value("NegInfinity", FloatLiteralType::NegInfinity, "Equivalent to float('-inf')")
    ;

    py::enum_<TokenType>(m, "TokenType")
        .value("Invalid", TokenType::Invalid)
        .value("Comment", TokenType::Comment)
        .value("Identifier", TokenType::Identifier)
        .value("True_", TokenType::True)
        .value("False_", TokenType::False)
        .value("Null", TokenType::Null)
        .value("Number", TokenType::Number)
        .value("String", TokenType::String)
        .value("ByteString", TokenType::ByteString)
        .value("DateTime", TokenType::DateTime)
        .value("Colon", TokenType::Colon)
        .value("Equals", TokenType::Equals)
        .value("Comma", TokenType::Comma)
        .value("Period", TokenType::Period)
        .value("BraceOpen", TokenType::BraceOpen)
        .value("BraceClose", TokenType::BraceClose)
        .value("SquareBracketOpen", TokenType::SquareBracketOpen)
        .value("SquareBracketClose", TokenType::SquareBracketClose)
        .value("AngleBracketOpen", TokenType::AngleBracketOpen)
        .value("AngleBracketClose", TokenType::AngleBracketClose)
        .value("ParenOpen", TokenType::ParenOpen)
        .value("ParenClose", TokenType::ParenClose)
        .value("ExclamationPoint", TokenType::ExclamationPoint)
        .value("Asterisk", TokenType::Asterisk)
        .value("QuestionMark", TokenType::QuestionMark)
        .value("AtSymbol", TokenType::AtSymbol)
        .value("Pipe", TokenType::Pipe)
        .value("Ampersand", TokenType::Ampersand)
        .value("Percent", TokenType::Percent)
        .value("Semicolon", TokenType::Semicolon)
        .value("Plus", TokenType::Plus)
        .value("Minus", TokenType::Minus)
        .value("Slash", TokenType::Slash)
        .value("Backslash", TokenType::Backslash)
        .value("Caret", TokenType::Caret)
        .value("Tilde", TokenType::Tilde)
        .value("Backtick", TokenType::Backtick)
        .value("LineBreak", TokenType::LineBreak)
        .value("EndOfStream", TokenType::EndOfStream)
    ;

    m.def("token_type_to_symbol", &token_type_to_symbol);
    m.def("token_type_from_symbol",
        [](py::str symbol) { return token_type_from_symbol(py::cast<std::string_view>(symbol)); },
        py::arg("symbol"));
    m.def("token_type_has_value", &token_type_has_value);

    auto token_cls = py::class_<Token>(m, "Token")
        .def(py::init([](TokenType type, py::object value, size_t start_idx, size_t end_idx, py::object tag) -> Token
        {
            return Token(type, start_idx, end_idx,
                value.is_none() ? FlexString() : py::cast<FlexString>(value),
                tag.is_none() ? FlexString() : py::cast<FlexString>(tag));
        }),
            py::arg("type"),
            py::arg("value") = py::none(),
            py::arg("start_idx") = invalid_idx,
            py::arg("end_idx") = invalid_idx,
            py::arg("tag") = py::none())
        .def_readwrite("type", &Token::type)
        .def_readwrite("value", &Token::value)
        .def_readwrite("tag", &Token::tag)
        .def_readwrite("start_idx", &Token::start_idx)
        .def_readwrite("end_idx", &Token::end_idx)

        .def("__eq__", &Token::operator==)
        .def("__ne__", &Token::operator!=)
        .def("__str__", &Token::to_string)
        .def("__repr__", &Token::to_repr)

        .def("copy", &Token::copy)

        .def("get_line_and_col", [](const Token& self, std::string_view buf)
        {
            size_t line = 0;
            size_t col = 0;
            if (self.get_line_and_col(buf, line, col))
            {
                return py::make_tuple(line, col);
            }
            throw py::value_error(
                jxc::format("Failed to get line and col from token: "
                "Index range {}..{} is out of range for buffer of size {}",
                self.start_idx, self.end_idx, buf.size()));
        })
    ;

    token_cls.attr("__match_args__") = py::make_tuple("type", "value");

    bind_tokenspan<TokenSpan>(m, "TokenSpan");

    bind_tokenspan<OwnedTokenSpan>(m, "OwnedTokenSpan")
        .def(py::init([](py::args args, py::str source) -> OwnedTokenSpan
        {
            OwnedTokenSpan result{};
            for (const auto& arg : args)
            {
                result.tokens.push(py::cast<Token>(arg));
            }
            if (py::len(source) > 0)
            {
                result.src = py::cast<FlexString>(source);
            }
            return result;
        }),
            py::arg("source") = "")
        .def("reset", &OwnedTokenSpan::reset)
        .def("__setitem__", [](OwnedTokenSpan& self, int64_t idx, const Token& tok)
        {
            self.tokens[detail::python_index(idx, self.tokens.size())] = tok;
        })
        .def("append", [](OwnedTokenSpan& self, const Token& tok) { self.tokens.push(tok); })
        .def("pop_back", [](OwnedTokenSpan& self) -> py::object
        {
            if (self.tokens.size() > 0)
            {
                auto result = py::cast(self.tokens.back().copy());
                self.tokens.pop_back();
                return result;
            }
            return py::none();
        })
    ;

    py::class_<PyLexer>(m, "Lexer")
        .def(py::init<py::str>())
        .def("next", &PyLexer::next)
        .def("get_error", &PyLexer::get_error)
        .def("get_token_pos", &PyLexer::get_token_pos)
    ;
    
    py::class_<PyAnnotationLexer>(m, "AnnotationLexer")
        .def(py::init<py::str>())
        .def("next", &PyAnnotationLexer::next)
        .def("has_error", &PyAnnotationLexer::has_error)
        .def("get_error_message", &PyAnnotationLexer::get_error_message)
    ;

    py::class_<ErrorInfo>(m, "ErrorInfo")
        .def(py::init())
        .def(py::init<std::string&&, size_t, size_t>(),
            py::arg("message"),
            py::arg("start_idx") = jxc::invalid_idx,
            py::arg("end_idx") = jxc::invalid_idx)

        .def_readwrite("is_err", &ErrorInfo::is_err)
        .def_readwrite("message", &ErrorInfo::message)
        .def_readwrite("buffer_start_idx", &ErrorInfo::buffer_start_idx)
        .def_readwrite("buffer_end_idx", &ErrorInfo::buffer_end_idx)
        .def_readwrite("line", &ErrorInfo::line)
        .def_readwrite("col", &ErrorInfo::col)

        .def("__bool__", &ErrorInfo::operator bool)
        .def("__str__", [](const ErrorInfo& self) { return self.to_string(); })
        .def("__repr__", [](const ErrorInfo& self) { return jxc::format("ErrorInfo({})", self.to_string()); })

        .def("get_line_and_col_from_buffer", &ErrorInfo::get_line_and_col_from_buffer)
        .def("reset", &ErrorInfo::reset)
        .def("to_string", &ErrorInfo::to_string, py::arg("buffer") = py::str(""))
    ;

    py::enum_<ElementType>(m, "ElementType")
        .value("Invalid", ElementType::Invalid)
        .value("Number", ElementType::Number)
        .value("Bool", ElementType::Bool)
        .value("Null", ElementType::Null)
        .value("Bytes", ElementType::Bytes)
        .value("String", ElementType::String)
        .value("DateTime", ElementType::DateTime)
        .value("ExpressionToken", ElementType::ExpressionToken)
        .value("Comment", ElementType::Comment)
        .value("BeginArray", ElementType::BeginArray)
        .value("EndArray", ElementType::EndArray)
        .value("BeginExpression", ElementType::BeginExpression)
        .value("EndExpression", ElementType::EndExpression)
        .value("BeginObject", ElementType::BeginObject)
        .value("ObjectKey", ElementType::ObjectKey)
        .value("EndObject", ElementType::EndObject)
    ;

    m.def("element_can_contain_value", &element_can_contain_value,
        py::doc(
            "Returns true if a given element type can contain one or more value tokens. "
            "(eg. a Number element always contains a value token, but a Null element does not require one)"));

    m.def("element_can_contain_annotation", &element_can_contain_annotation,
        py::doc("Returns true if a given element type is allowed to have an annotation"));

    m.def("element_is_value_type", &element_is_value_type,
        py::doc("Returns true if a given element is a valid value type (includes container-begin elements)"));

    m.def("element_is_expression_value_type", &element_is_expression_value_type,
        py::doc("Returns true if a given element is valid for use inside an expression container"));

    bind_element<Element>(m, "Element");
    bind_element<OwnedElement>(m, "OwnedElement");

    m.def("is_debug_build", []() -> bool
    {
#if JXC_DEBUG
        return true;
#else
        return false;
#endif
    },
        py::doc("Returns True if pyjxc was compiled with JXC_DEBUG=1"));

    m.def("is_profiler_enabled", &jxc::is_profiler_enabled,
        py::doc("Returns True if pyjxc was compiled with JXC_ENABLE_JUMP_BLOCK_PROFILER=1"));

    py::class_<JumpParser>(m, "JumpParser")
        .def(py::init<std::string_view>())
        .def("reset", &JumpParser::reset)
        .def("next", &JumpParser::next)
        .def("value", &JumpParser::value)
        .def("has_error", &JumpParser::has_error)
        .def("stack_depth", &JumpParser::stack_depth)
        .def("get_error", &JumpParser::get_error)
        .def_static("reset_profiler", &JumpParser::reset_profiler,
            py::doc("Requires compilation with JXC_ENABLE_JUMP_BLOCK_PROFILER=1"))
        .def_static("get_profiler_results", &JumpParser::get_profiler_results,
            py::arg("sort_by_runtime") = true,
            py::doc("Requires compilation with JXC_ENABLE_JUMP_BLOCK_PROFILER=1"))
    ;

    m.def("split_number_token_value", [](const Token& number_token) -> py::object
    {
        jxc::util::NumberTokenSplitResult number;
        ErrorInfo err;
        if (jxc::util::split_number_token_value(number_token, number, err))
        {
            return py::make_tuple(number.sign, number.prefix, number.value, number.exponent, number.suffix, number.float_type);
        }
        else
        {
            return py::cast(err);
        }
    }, py::doc(R"DOCSTRING(
        Splits a number token into its component parts, and returns a tuple in the form (sign, prefix, number, exponent, suffix, float_type).
        All return types are strings except exponent, which is an int.
    )DOCSTRING"));

    m.def("parse_string_token", [](const Token& string_token) -> py::object
    {
        if (string_token.type != TokenType::String)
        {
            throw py::value_error(jxc::format("Expected String token, got {}", token_type_to_string(string_token.type)));
        }

        ErrorInfo err;
        std::string result;
        if (util::parse_string_token(string_token, result, err))
        {
            return py::cast(result);
        }
        else
        {
            return py::cast(err);
        }
    });

    m.def("parse_string_token", [](py::str string_value) -> py::str
    {
        const Token tok = Token(TokenType::String, invalid_idx, invalid_idx, py::cast<FlexString>(string_value));
        ErrorInfo err;
        std::string result;
        if (util::parse_string_token(tok, result, err))
        {
            return py::cast(result);
        }
        else
        {
            throw py::value_error(jxc::format("Failed parsing string token: {}", err.to_string()));
        }
    });

    m.def("parse_number_token", [](const Token& number_token) -> py::tuple
    {
        if (number_token.type != TokenType::Number)
        {
            throw py::value_error(jxc::format("Expected Number token, got {}", token_type_to_string(number_token.type)));
        }

        ErrorInfo err;
        util::NumberTokenSplitResult number;
        if (!util::split_number_token_value(number_token, number, err))
        {
            return py::make_tuple(err, py::str(""));
        }

        if (number.is_integer())
        {
            int64_t int_value = 0;
            if (!util::parse_number<int64_t>(number_token, int_value, number, err))
            {
                return py::make_tuple(err, number.suffix);
            }
            return py::make_tuple(py::int_(int_value), number.suffix);
        }
        else
        {
            double float_value = 0.0;
            if (!util::parse_number<double>(number_token, float_value, number, err))
            {
                return py::make_tuple(err, number.suffix);
            }
            return py::make_tuple(py::float_(float_value), number.suffix);
        }
    },
        py::arg("number_token"),
        py::doc("Parses a number token to a float or int value. Always returns a 2-tuple in the form (value_or_errorinfo, number_suffix)"));

    m.def("parse_bytes_token", [](const Token& bytes_token) -> py::object
    {
        if (bytes_token.type != TokenType::ByteString)
        {
            throw py::value_error(jxc::format("Expected ByteString token, got {}", token_type_to_string(bytes_token.type)));
        }

        py::bytearray result{};
        std::string_view value = bytes_token.value.as_view();
        const size_t num_bytes = jxc::util::get_byte_buffer_required_size(value.data(), value.size());
        if (PyByteArray_Resize(result.ptr(), num_bytes) != 0)
        {
            throw py::error_already_set();
        }
        uint8_t* byte_buffer = reinterpret_cast<uint8_t*>(PyByteArray_AsString(result.ptr()));
        if (byte_buffer == nullptr)
        {
            throw py::error_already_set();
        }

        ErrorInfo error;
        size_t num_bytes_written = 0;
        if (util::parse_bytes_token(bytes_token, byte_buffer, num_bytes, num_bytes_written, error))
        {
            return result;
        }
        else
        {
            return py::cast(error);
        }
    });

    m.def("datetime_token_is_date", &util::datetime_token_is_date,
        py::arg("token"),
        py::doc("Checks if the given datetime token is just a date (no time data)"));

    m.def("datetime_token_is_datetime", &util::datetime_token_is_datetime,
        py::arg("token"),
        py::doc("Checks if the given datetime token includes both date and time data"));
    
    m.def("date_to_iso8601", &jxc::date_to_iso8601, py::arg("dt"));
    m.def("datetime_to_iso8601", &jxc::datetime_to_iso8601, py::arg("dt"), py::arg("auto_strip_time") = false);

    m.def("parse_date_token", [](const Token& date_token)
    {
        jxc::Date result;
        jxc::ErrorInfo err;
        if (util::parse_date_token(date_token, result, err))
        {
            return py::cast(result);
        }
        else
        {
            return py::cast(err);
        }
    },
        py::arg("token"),
        py::doc("Parses a date token into a Date value. This will return an error if the token includes time data."));

    m.def("parse_datetime_token", [](const Token& datetime_token, bool require_time_data)
    {
        jxc::DateTime result;
        jxc::ErrorInfo err;
        if (util::parse_datetime_token(datetime_token, result, err, require_time_data))
        {
            return py::cast(result);
        }
        else
        {
            return py::cast(err);
        }
    },
        py::arg("token"),
        py::arg("require_time_data") = false,
        py::doc(R"DOC(Parses a date token into a DateTime value.
If require_time_data is true, this will return an error if the token does not include time info.
If require_time_data is false and the token does not include time data, out_datetime will have a time of 00:00:00Z.)DOC"));

    m.def("is_valid_identifier", &is_valid_identifier);
    m.def("is_valid_identifier_first_char", &is_valid_identifier_first_char);
    m.def("is_valid_identifier_char", &is_valid_identifier_char);
    m.def("is_valid_object_key", &is_valid_object_key);

    m.def("debug_string_repr", &detail::debug_string_repr, py::arg("value"), py::arg("quote_char") = '"',
        py::doc("helper for creating a debug representation for a string"));

    m.def("debug_bytes_repr", [](py::bytes value, char quote_char) -> std::string
        {
            std::string_view data = py::cast<std::string_view>(value);
            return detail::debug_bytes_repr(BytesView(reinterpret_cast<const uint8_t*>(data.data()), data.size()), quote_char);
        },
        py::arg("value"),
        py::arg("quote_char") = '"',
        py::doc("helper for creating a debug representation for bytes"));

    m.def("debug_char_repr", [](py::object value, char quote_char) -> std::string
        {
            return detail::debug_char_repr(python_value_to_codepoint(value), quote_char);
        },
        py::arg("codepoint"),
        py::arg("quote_char") = '`',
        py::doc("helper for creating a debug representation for a single codepoint"));

    m.def("is_ascii_escape_char", [](py::object value, char quote_char) -> bool
        {
            return detail::is_ascii_escape_char(python_value_to_codepoint(value), quote_char);
        },
        py::arg("codepoint"),
        py::arg("quote_char") = '\0',
        py::doc("checks if a character should be escaped as ascii (eg. '\t')"));

    m.def("is_renderable_ascii_char", [](py::object value) -> bool
        {
            return detail::is_renderable_ascii_char(python_value_to_codepoint(value));
        },
        py::arg("codepoint"),
        py::doc("Checks if a given codepoint is a renderable ASCII char (eg. not a control character). NB: Returns true for space."));

    py::enum_<ExpressionParseMode>(m, "ExpressionParseMode")
        .value("ValueList", ExpressionParseMode::ValueList,
            "Returns the expression as a list of values (excluding comments)")
        .value("ValueListWithComments", ExpressionParseMode::ValueListWithComments,
            "Returns the expression as a list of values (including comments)")
        .value("TokenList", ExpressionParseMode::TokenList,
            "Returns the expression as a list of Token objects")
        .value("SourceString", ExpressionParseMode::SourceString,
            "Returns the expression as a string, with the original formatting intact")
    ;

    py::enum_<ClassConstructMode>(m, "ClassConstructMode")
        .value("Auto", ClassConstructMode::Auto,
            "If the value is a list, pass values as `*args`. If the value is a dict, pass values as `** kwargs`. "
            "Otherwise pass a single value as the first argument to the class constructor.")
        .value("Value", ClassConstructMode::Value,
            "Pass any value with the class's annotation as one argument to the class constructor")
        .value("ListAsArgs", ClassConstructMode::ListAsArgs,
            "Require that the class value is a list, and pass through all list values as `*args` to the class constructor")
        .value("DictAsKeywordArgs", ClassConstructMode::DictAsKeywordArgs,
            "Require that the class value is a dict, and pass through all dict key/value pairs as `**kwargs` to the class constructor")
    ;

    py::class_<PyParser>(m, "Parser")
        .def(py::init<py::object, ExpressionParseMode, bool, bool>(),
            py::arg("buf"),
            py::kw_only{},
            py::arg("default_expr_parse_mode") = ExpressionParseMode::ValueList,
            py::arg("ignore_unknown_annotations") = true,
            py::arg("ignore_unknown_number_suffixes") = true)

        .def_property("default_expr_parse_mode", &PyParser::get_default_expr_parse_mode, &PyParser::set_default_expr_parse_mode)
        .def_property("ignore_unknown_annotations", &PyParser::get_ignore_unknown_annotations, &PyParser::set_ignore_unknown_annotations)
        .def_property("ignore_unknown_number_suffixes", &PyParser::get_ignore_unknown_number_suffixes, &PyParser::set_ignore_unknown_number_suffixes)

        .def("set_annotation_constructor", &PyParser::set_annotation_constructor,
            py::arg("annotation"), py::arg("construct"))
        .def("set_number_suffix_constructor", &PyParser::set_number_suffix_constructor,
            py::arg("suffix"), py::arg("construct"))
        .def("set_find_construct_from_annotation_callback", &PyParser::set_find_construct_from_annotation_callback,
            py::arg("callback"))
        .def("set_find_construct_from_number_suffix_callback", &PyParser::set_find_construct_from_number_suffix_callback,
            py::arg("callback"))

        .def("set_custom_list_type", &PyParser::set_custom_list_type, py::arg("list_type"))
        .def("set_custom_dict_type", &PyParser::set_custom_dict_type, py::arg("dict_type"))

        .def("parse", &PyParser::parse)
        .def("has_error", &PyParser::has_error)
        .def("get_error", &PyParser::get_error)
    ;

    py::enum_<StringQuoteMode>(m, "StringQuoteMode")
        .value("Auto", StringQuoteMode::Auto)
        .value("Double", StringQuoteMode::Double)
        .value("Single", StringQuoteMode::Single)
    ;

    py::class_<SerializerSettings>(m, "SerializerSettings")
        .def(py::init([](
            bool pretty_print,
            int32_t target_line_length,
            const std::string& indent,
            const std::string& linebreak,
            const std::string& key_separator,
            const std::string& value_separator,
            StringQuoteMode default_quote,
            int32_t default_float_precision,
            bool float_fixed_precision
            ) -> SerializerSettings
        {
            SerializerSettings result;
            result.pretty_print = pretty_print;
            result.target_line_length = target_line_length;
            result.indent = indent;
            result.linebreak = linebreak;
            result.key_separator = key_separator;
            result.value_separator = value_separator;
            result.default_quote = default_quote;
            result.default_float_precision = default_float_precision;
            result.float_fixed_precision = float_fixed_precision;
            return result;
        }), 
            py::kw_only{},
            py::arg("pretty_print") = true,
            py::arg("target_line_length") = 80,
            py::arg("indent") = "    ",
            py::arg("linebreak") = "\n",
            py::arg("key_separator") = ": ",
            py::arg("value_separator") = ",\n",
            py::arg("default_quote") = StringQuoteMode::Double,
            py::arg("default_float_precision") = 12,
            py::arg("float_fixed_precision") = false)
        
        .def_static("make_compact", &SerializerSettings::make_compact)

        .def_readwrite("pretty_print", &SerializerSettings::pretty_print)
        .def_readwrite("target_line_length", &SerializerSettings::target_line_length)
        .def_readwrite("indent", &SerializerSettings::indent)
        .def_readwrite("linebreak", &SerializerSettings::linebreak)
        .def_readwrite("key_separator", &SerializerSettings::key_separator)
        .def_readwrite("value_separator", &SerializerSettings::value_separator)
        .def_readwrite("default_quote", &SerializerSettings::default_quote)
        .def_readwrite("default_float_precision", &SerializerSettings::default_float_precision)
        .def_readwrite("float_fixed_precision", &SerializerSettings::float_fixed_precision)
        .def("get_target_line_length", &SerializerSettings::get_target_line_length)
        .def("__repr__", &SerializerSettings::to_repr)
    ;

    py::class_<PySerializer>(m, "Serializer")
        .def(py::init<const jxc::SerializerSettings&>(), py::arg("settings") = jxc::SerializerSettings{})
        .def("get_settings", &PySerializer::get_settings)
        .def("clear", &PySerializer::clear)
        .def("done", &PySerializer::done)
        .def("flush", &PySerializer::flush)
        .def("get_result", &PySerializer::get_result)
        .def("__str__", &PySerializer::get_result)
        .def("annotation", &PySerializer::annotation)
        .def("value_null", &PySerializer::value_null)
        .def("value_bool", &PySerializer::value_bool)
        .def("value_int", &PySerializer::value_int, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_int_hex", &PySerializer::value_int_hex, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_int_oct", &PySerializer::value_int_oct, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_int_bin", &PySerializer::value_int_bin, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_nan", &PySerializer::value_nan)
        .def("value_inf", &PySerializer::value_inf, py::arg("negative") = false)
        .def("value_float", &PySerializer::value_float, py::arg("value"), py::arg("suffix") = std::string_view{}, py::arg("precision") = 16, py::arg("fixed") = false)
        .def("value_string", &PySerializer::value_string, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto, py::arg("decode_unicode") = true)
        .def("value_string_raw", &PySerializer::value_string_raw, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto, py::arg("tag") = std::string_view{})
        .def("value_bytes", [](PySerializer& self, py::bytes value, StringQuoteMode quote)
        {
            auto data = py::cast<std::string_view>(value);
            self.value_bytes(reinterpret_cast<const uint8_t*>(data.data()), data.size(), quote);
            return self;
        },
            py::arg("value"),
            py::arg("quote") = StringQuoteMode::Auto)
        .def("value_bytes_base64", [](PySerializer& self, py::bytes value, StringQuoteMode quote)
        {
            auto data = py::cast<std::string_view>(value);
            self.value_bytes_base64(reinterpret_cast<const uint8_t*>(data.data()), data.size(), quote);
            return self;
        },
            py::arg("value"),
            py::arg("quote") = StringQuoteMode::Auto)
        .def("value_date", &PySerializer::value_date, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto)
        .def("value_datetime", &PySerializer::value_datetime, py::arg("value"), py::arg("auto_strip_time") = false, py::arg("quote") = StringQuoteMode::Auto)
        .def("identifier", &PySerializer::identifier)
        .def("identifier_or_string", &PySerializer::identifier_or_string, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto, py::arg("decode_unicode") = true)
        .def("comment", &PySerializer::comment)
        .def("write", [](PySerializer& self, py::str value)
        {
            const auto s = py::cast<std::string_view>(value);
            self.write(s);
            return self;
        })
        .def("array_begin", &PySerializer::array_begin, py::arg("separator") = std::string_view{})
        .def("array_end", &PySerializer::array_end)
        .def("array_empty", &PySerializer::array_empty)
        .def("expression_begin", &PySerializer::expression_begin)
        .def("expression_end", &PySerializer::expression_end)
        .def("expression_empty", &PySerializer::expression_empty)
        .def("sep", &PySerializer::sep)
        .def("object_begin", &PySerializer::object_begin, py::arg("separator") = std::string_view{})
        .def("object_sep", &PySerializer::object_sep)
        .def("object_end", &PySerializer::object_end)
        .def("object_empty", &PySerializer::object_empty)
    ;

    py::class_<ExpressionProxy>(m, "ExpressionProxy")
        .def("value_null", &ExpressionProxy::value_null)
        .def("value_bool", &ExpressionProxy::value_bool)
        .def("value_int", &ExpressionProxy::value_int, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_int_hex", &ExpressionProxy::value_int_hex, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_int_oct", &ExpressionProxy::value_int_oct, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_int_bin", &ExpressionProxy::value_int_bin, py::arg("value"), py::arg("suffix") = std::string_view{})
        .def("value_nan", &ExpressionProxy::value_nan)
        .def("value_inf", &ExpressionProxy::value_inf, py::arg("negative") = false)
        .def("value_float", &ExpressionProxy::value_float, py::arg("value"), py::arg("suffix") = std::string_view{}, py::arg("precision") = 8, py::arg("fixed") = false)
        .def("value_string", &ExpressionProxy::value_string, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto, py::arg("decode_unicode") = true)
        .def("value_string_raw", &ExpressionProxy::value_string_raw, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto)
        .def("value_bytes", [](ExpressionProxy& self, py::bytes value, StringQuoteMode quote)
        {
            auto data = py::cast<std::string_view>(value);
            self.value_bytes(reinterpret_cast<const uint8_t*>(data.data()), data.size(), quote);
            return self;
        },
            py::arg("value"),
            py::arg("quote") = StringQuoteMode::Auto)
        .def("value_bytes_base64", [](ExpressionProxy& self, py::bytes value, StringQuoteMode quote)
        {
            auto data = py::cast<std::string_view>(value);
            return self.value_bytes_base64(reinterpret_cast<const uint8_t*>(data.data()), data.size(), quote);
        },
            py::arg("value"),
            py::arg("quote") = StringQuoteMode::Auto)
        .def("value_date", &ExpressionProxy::value_date, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto)
        .def("value_datetime", &ExpressionProxy::value_datetime, py::arg("value"), py::arg("auto_strip_time") = false, py::arg("quote") = StringQuoteMode::Auto)
        .def("identifier", &ExpressionProxy::identifier)
        .def("identifier_or_string", &ExpressionProxy::identifier_or_string, py::arg("value"), py::arg("quote") = StringQuoteMode::Auto, py::arg("decode_unicode") = true)
        .def("op", &ExpressionProxy::op)
        .def("write", [](ExpressionProxy& self, py::str value)
        {
            const auto s = py::cast<std::string_view>(value);
            return self.write(s);
        })
        .def("token", &ExpressionProxy::token)
        .def("comma", &ExpressionProxy::comma)
        .def("paren_open", &ExpressionProxy::paren_open)
        .def("paren_close", &ExpressionProxy::paren_close)
        .def("expression_end", &ExpressionProxy::expression_end)
    ;

    py::class_<PyEncoder>(m, "Encoder")
        .def(py::init<const SerializerSettings&, bool, bool, bool, bool>(),
            py::arg("settings"),
            py::kw_only{},
            py::arg("encode_inline") = false,
            py::arg("sort_keys") = false,
            py::arg("skip_keys") = false,
            py::arg("decode_unicode") = true)

        .def_property_readonly("doc", &PyEncoder::get_serializer, py::return_value_policy::reference_internal)

        .def("set_find_encoder_callback", &PyEncoder::set_find_encoder_callback)
        .def("set_find_fallback_encoder_callback", &PyEncoder::set_find_fallback_encoder_callback)

        .def("encode_sequence", &PyEncoder::encode_sequence)
        .def("encode_dict", &PyEncoder::encode_dict)
        .def("encode_value", &PyEncoder::encode_value)

        .def("get_result", [](PyEncoder& self) -> py::str { return self.get_serializer().get_result(); })
        .def("get_result_bytes", [](PyEncoder& self) -> py::bytes { return self.get_serializer().get_result_bytes(); })
    ;

}
