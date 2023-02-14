#include "jxc_core_tests.h"


struct TestJumpParser
{
    std::string jxc_string;
    std::shared_ptr<jxc::JumpParser> parser;
    int64_t element_index = 0;
    bool done = false;

    TestJumpParser(std::string_view buf)
        : jxc_string(std::string(buf))
        , parser(std::make_shared<jxc::JumpParser>(jxc_string))
    {
    }

    ~TestJumpParser()
    {
        if (parser && !done && parser->next())
        {
            JXC_ASSERTF(parser->value().type == jxc::ElementType::Invalid,
                "TestJumpParser({}) destroyed before testing all values", jxc::detail::debug_string_repr(jxc_string));
        }
    }

    testing::AssertionResult next(const jxc::OwnedElement& expected_element)
    {
        using namespace jxc;

        if (done)
        {
            return testing::AssertionFailure() << "Parsing already done";
        }

        auto make_parse_error = [this]()
        {
            ErrorInfo err = parser->get_error();
            return testing::AssertionFailure() << "Error parsing " << detail::debug_string_repr(jxc_string, '`')
                << ": " << err.to_string(jxc_string);
        };

        if (parser->next())
        {
            // if next() returns true, there should not be an error
            if (parser->has_error())
            {
                return testing::AssertionFailure() << "JumpParser returned true but also has an error: "
                    << parser->get_error().to_string(jxc_string);
            }

            ++element_index;
            const Element& parsed_element = parser->value();
            auto result = test_elements_equal(parsed_element, expected_element);
            if (!result)
            {
                return result;
            }
            else
            {
                return testing::AssertionSuccess();
            }
        }
        else
        {
            done = true;

            if (parser->has_error())
            {
                return make_parse_error();
            }
            else
            {
                return testing::AssertionSuccess();
            }
        }
    }
};


testing::AssertionResult _jxc_expect_jump_parse_next(
    const char* parser_str, const char* expected_element_str,
    TestJumpParser& parser, const jxc::OwnedElement& expected_element)
{
    return parser.next(expected_element);
}


#define EXPECT_PARSE_NEXT(PARSER, EXPECTED_ELEMENT) EXPECT_PRED_FORMAT2(_jxc_expect_jump_parse_next, (PARSER), (EXPECTED_ELEMENT))


#define EXPECT_PARSE_SINGLE(JXC_VALUE, EXPECTED_ELEMENT) do { \
    TestJumpParser parser(JXC_VALUE); \
    EXPECT_PRED_FORMAT2(_jxc_expect_jump_parse_next, parser, (EXPECTED_ELEMENT)); \
} while(0)


testing::AssertionResult _jxc_expect_jump_parse_error(
    const char* jxc_value_str, const char* error_substring_match_str,
    const std::string& jxc_value, const std::string& error_substring_match)
{
    TestJumpParser parser(jxc_value);

    // skip to end, we don't care about any values, we just want to see if an error occurs
    while (parser.parser->next())
    {
    }

    if (parser.parser->has_error())
    {
        const jxc::ErrorInfo& err = parser.parser->get_error();
        JXC_ASSERT(err.is_err);
        if (err.message.find_first_of(error_substring_match) != std::string::npos)
        {
            return testing::AssertionSuccess();
        }
        else
        {
            return testing::AssertionFailure()
                << "Parser does contain an error, as expected, but the error message does not match. "
                << "\n\tError message: " << jxc::detail::debug_string_repr(err.message)
                << "\n\tRequired substring: " << jxc::detail::debug_string_repr(error_substring_match);
        }
    }
    else
    {
        return testing::AssertionFailure()
            << "Parse successful, but an error matching "
            << jxc::detail::debug_string_repr(error_substring_match)
            << " was expected";
    }
}


#define EXPECT_PARSE_FAILURE(JXC_VALUE, EXPECTED_ERROR_SUBSTRING) EXPECT_PRED_FORMAT2(_jxc_expect_jump_parse_error, (JXC_VALUE), (EXPECTED_ERROR_SUBSTRING))


template<typename Lambda>
std::string test_serialize(Lambda&& callback)
{
    jxc::StringOutputBuffer output;
    jxc::Serializer doc(&output, jxc::SerializerSettings::make_compact());
    callback(doc);
    doc.flush();
    return output.to_string();
}


TEST(jxc_core, JumpParserSimple)
{
    using namespace jxc;

    // null
    EXPECT_PARSE_SINGLE("null", make_element(ElementType::Null, make_token(TokenType::Null)));

    // bool
    EXPECT_PARSE_SINGLE("true", make_element(ElementType::Bool, make_token(TokenType::True)));
    EXPECT_PARSE_SINGLE("false", make_element(ElementType::Bool, make_token(TokenType::False)));

    // integers (dec)
    EXPECT_PARSE_SINGLE("0", make_element(ElementType::Number, make_token(TokenType::Number, "0")));
    EXPECT_PARSE_SINGLE("1", make_element(ElementType::Number, make_token(TokenType::Number, "1")));
    EXPECT_PARSE_SINGLE("2", make_element(ElementType::Number, make_token(TokenType::Number, "2")));
    EXPECT_PARSE_SINGLE("-1", make_element(ElementType::Number, make_token(TokenType::Number, "-1")));
    EXPECT_PARSE_SINGLE("-2", make_element(ElementType::Number, make_token(TokenType::Number, "-2")));

    // integers (hex)
    EXPECT_PARSE_SINGLE("0x0", make_element(ElementType::Number, make_token(TokenType::Number, "0x0")));
    EXPECT_PARSE_SINGLE("0xaf", make_element(ElementType::Number, make_token(TokenType::Number, "0xaf")));
    EXPECT_PARSE_SINGLE("-0xf", make_element(ElementType::Number, make_token(TokenType::Number, "-0xf")));

    // integers (oct)
    EXPECT_PARSE_SINGLE("0o0", make_element(ElementType::Number, make_token(TokenType::Number, "0o0")));
    EXPECT_PARSE_SINGLE("0o777", make_element(ElementType::Number, make_token(TokenType::Number, "0o777")));
    EXPECT_PARSE_SINGLE("-0o644", make_element(ElementType::Number, make_token(TokenType::Number, "-0o644")));

    // integers (bin)
    EXPECT_PARSE_SINGLE("0b0", make_element(ElementType::Number, make_token(TokenType::Number, "0b0")));
    EXPECT_PARSE_SINGLE("0b1", make_element(ElementType::Number, make_token(TokenType::Number, "0b1")));
    EXPECT_PARSE_SINGLE("0b1101", make_element(ElementType::Number, make_token(TokenType::Number, "0b1101")));
    EXPECT_PARSE_SINGLE("-0b1", make_element(ElementType::Number, make_token(TokenType::Number, "-0b1")));
    EXPECT_PARSE_SINGLE("-0b101", make_element(ElementType::Number, make_token(TokenType::Number, "-0b101")));

    // floats
    EXPECT_PARSE_SINGLE("0.0", make_element(ElementType::Number, make_token(TokenType::Number, "0.0")));
    EXPECT_PARSE_SINGLE("0.5", make_element(ElementType::Number, make_token(TokenType::Number, "0.5")));
    EXPECT_PARSE_SINGLE("-0.5", make_element(ElementType::Number, make_token(TokenType::Number, "-0.5")));
    EXPECT_PARSE_SINGLE("123.456", make_element(ElementType::Number, make_token(TokenType::Number, "123.456")));
    EXPECT_PARSE_SINGLE("-123.456", make_element(ElementType::Number, make_token(TokenType::Number, "-123.456")));

    // float literals
    EXPECT_PARSE_SINGLE("nan", make_element(ElementType::Number, make_token(TokenType::Number, "nan")));
    EXPECT_PARSE_SINGLE("+inf", make_element(ElementType::Number, make_token(TokenType::Number, "+inf")));
    EXPECT_PARSE_SINGLE("-inf", make_element(ElementType::Number, make_token(TokenType::Number, "-inf")));

    // normal strings
    EXPECT_PARSE_SINGLE("\"\"", make_element(ElementType::String, make_token(TokenType::String, "\"\"")));
    EXPECT_PARSE_SINGLE("''", make_element(ElementType::String, make_token(TokenType::String, "''")));
    EXPECT_PARSE_SINGLE("\"abc\"", make_element(ElementType::String, make_token(TokenType::String, "\"abc\"")));
    EXPECT_PARSE_SINGLE("'abc'", make_element(ElementType::String, make_token(TokenType::String, "'abc'")));

    // raw strings
    EXPECT_PARSE_SINGLE("r\"()\"", make_element(ElementType::String, make_token(TokenType::String, "r\"()\"")));
    EXPECT_PARSE_SINGLE("r'()'", make_element(ElementType::String, make_token(TokenType::String, "r'()'")));
    EXPECT_PARSE_SINGLE("r\"(abc)\"", make_element(ElementType::String, make_token(TokenType::String, "r\"(abc)\"")));
    EXPECT_PARSE_SINGLE("r'(abc)'", make_element(ElementType::String, make_token(TokenType::String, "r'(abc)'")));

    // raw strings (with heredoc)
    EXPECT_PARSE_SINGLE("r\"HEREDOC()HEREDOC\"", make_element(ElementType::String, make_token(TokenType::String, "r\"HEREDOC()HEREDOC\"")));
    EXPECT_PARSE_SINGLE("r'HEREDOC()HEREDOC'", make_element(ElementType::String, make_token(TokenType::String, "r'HEREDOC()HEREDOC'")));
    EXPECT_PARSE_SINGLE("r\"HEREDOC(abc)HEREDOC\"", make_element(ElementType::String, make_token(TokenType::String, "r\"HEREDOC(abc)HEREDOC\"")));
    EXPECT_PARSE_SINGLE("r'HEREDOC(abc)HEREDOC'", make_element(ElementType::String, make_token(TokenType::String, "r'HEREDOC(abc)HEREDOC'")));

    // base64 strings
    EXPECT_PARSE_SINGLE("b64\"\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"\"")));
    EXPECT_PARSE_SINGLE("b64''", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64''")));
    EXPECT_PARSE_SINGLE("b64\"anhjIGZvcm1hdA==\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"anhjIGZvcm1hdA==\"")));
    EXPECT_PARSE_SINGLE("b64'anhjIGZvcm1hdA=='", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64'anhjIGZvcm1hdA=='")));

    // base64 strings (with parens - allows whitespace)
    EXPECT_PARSE_SINGLE("b64\"()\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"()\"")));
    EXPECT_PARSE_SINGLE("b64'()'", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64'()'")));
    EXPECT_PARSE_SINGLE("b64\"( anhjIGZ vcm1hdA== )\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"( anhjIGZ vcm1hdA== )\"")));
    EXPECT_PARSE_SINGLE("b64'( anhjIGZ vcm1hdA== )'", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64'( anhjIGZ vcm1hdA== )'")));
}


TEST(jxc_core, JumpParserArrays)
{
    using namespace jxc;

    {
        TestJumpParser parser("[]");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginArray, make_token(TokenType::SquareBracketOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndArray, make_token(TokenType::SquareBracketClose)));
    }

    {
        TestJumpParser parser("[null]");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginArray, make_token(TokenType::SquareBracketOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Null, make_token(TokenType::Null)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndArray, make_token(TokenType::SquareBracketClose)));
    }

    {
        TestJumpParser parser("[1, 2, 3]");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginArray, make_token(TokenType::SquareBracketOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "2")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "3")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndArray, make_token(TokenType::SquareBracketClose)));
    }

    {
        TestJumpParser parser("[null, 3.141, true]");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginArray, make_token(TokenType::SquareBracketOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Null, make_token(TokenType::Null)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "3.141")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bool, make_token(TokenType::True)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndArray, make_token(TokenType::SquareBracketClose)));
    }
}


TEST(jxc_core, JumpParserByteStrings)
{
    using namespace jxc;

    EXPECT_PARSE_SINGLE("b64\"anhj\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"anhj\"")));
    EXPECT_PARSE_SINGLE("b64\"( a n h j )\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"( a n h j )\"")));

    {
        TestJumpParser parser("[0xFF,b64\"anhj\",0xAA]");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginArray, make_token(TokenType::SquareBracketOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0xFF")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"anhj\"")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0xAA")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndArray, make_token(TokenType::SquareBracketClose)));
    }

    {
        TestJumpParser parser("[0xFF,b64\"(        a n h j         )\",0xAA]");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginArray, make_token(TokenType::SquareBracketOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0xFF")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bytes, make_token(TokenType::ByteString, "b64\"(        a n h j         )\"")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0xAA")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndArray, make_token(TokenType::SquareBracketClose)));
    }

    // invalid base64 strings
    EXPECT_PARSE_FAILURE("b64\"a\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"an\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"anh\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"( a )\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"(           a           n             )\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"( anh )\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"( a n h j = )\"", "length must be a multiple of 4");
    EXPECT_PARSE_FAILURE("b64\"(*)\"", "`*` is not a valid base64 character");
    EXPECT_PARSE_FAILURE("[0xFF,b64\"(        a n h j !         )\",0xAA]", "`!` is not a valid base64 character");
}


TEST(jxc_core, JumpParserExpressions)
{
    using namespace jxc;

    {
        TestJumpParser parser("()");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(1 + 1)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(true | false)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bool, make_token(TokenType::True)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Pipe, "|")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bool, make_token(TokenType::False)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(abc=def, qqq=(1 + 2 / 3 * 4), zzz~=5)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Identifier, "abc")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Equals, "=")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Identifier, "def")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Comma, ",")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Identifier, "qqq")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Equals, "=")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::ParenOpen, "(")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "2")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Slash, "/")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "3")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Asterisk, "*")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "4")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::ParenClose, ")")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Comma, ",")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Identifier, "zzz")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Tilde, "~")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Equals, "=")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "5")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }
}


TEST(jxc_core, JumpParserUnaryOperatorExpressions)
{
    using namespace jxc;

    // Expressions do not currently support unary operators.
    // It's very difficult to handle these in a way that makes sense for all use cases.
    // Need to do more research in this area.

    {
        TestJumpParser parser("(1 - -1)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(1+1)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(1-1)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(+1++1)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(--1--1--)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(-inf)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "-inf")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(+inf)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "+inf")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(--2++inf--inf+5)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "2")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "+inf")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Minus, "-")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "-inf")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Plus, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "5")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }
}


TEST(jxc_core, JumpParserObjects)
{
    using namespace jxc;

    {
        TestJumpParser parser("{}");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginObject, make_token(TokenType::BraceOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndObject, make_token(TokenType::BraceClose)));
    }

    {
        TestJumpParser parser("{null:null}");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginObject, make_token(TokenType::BraceOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::Null)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Null, make_token(TokenType::Null)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndObject, make_token(TokenType::BraceClose)));
    }

    {
        TestJumpParser parser("{x:0}");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginObject, make_token(TokenType::BraceOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::Identifier, "x")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndObject, make_token(TokenType::BraceClose)));
    }

    {
        TestJumpParser parser("{ x: 4.0, y: 0.0, z: -3.5 }");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginObject, make_token(TokenType::BraceOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::Identifier, "x")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "4.0")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::Identifier, "y")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0.0")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::Identifier, "z")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "-3.5")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndObject, make_token(TokenType::BraceClose)));
    }
}


TEST(jxc_core, JumpParserAnnotations)
{
    using namespace jxc;

    EXPECT_PARSE_SINGLE("annotation null", make_element(ElementType::Null, make_token(TokenType::Null),
        { make_token(TokenType::Identifier, "annotation") }));

    EXPECT_PARSE_SINGLE("array<int> null", make_element(ElementType::Null, make_token(TokenType::Null), {
        make_token(TokenType::Identifier, "array"),
        make_token(TokenType::AngleBracketOpen),
        make_token(TokenType::Identifier, "int"),
        make_token(TokenType::AngleBracketClose),
    }));

    EXPECT_PARSE_SINGLE("annotation<int, null, 4, true, regex=r'(\\w*)'> null", make_element(ElementType::Null, make_token(TokenType::Null), {
        make_token(TokenType::Identifier, "annotation"),
        make_token(TokenType::AngleBracketOpen),
        make_token(TokenType::Identifier, "int"),
        make_token(TokenType::Comma),
        make_token(TokenType::Null),
        make_token(TokenType::Comma),
        make_token(TokenType::Number, "4"),
        make_token(TokenType::Comma),
        make_token(TokenType::True),
        make_token(TokenType::Comma),
        make_token(TokenType::Identifier, "regex"),
        make_token(TokenType::Equals),
        make_token(TokenType::String, "r'(\\w*)'"),
        make_token(TokenType::AngleBracketClose),
    }));
}


testing::AssertionResult test_parse_number(
    const char* jxc_number_str,
    const char* split_result_str,
    const std::string& jxc_string,
    const jxc::util::NumberTokenSplitResult& expected_result)
{
    auto mkerr = [&]()
    {
        return jxc::format(" (expected {} to parse to {})", jxc_number_str, split_result_str);
    };

    jxc::Lexer lexer(jxc_string.data(), jxc_string.size());
    jxc::Token number_token;
    jxc::ErrorInfo err;

    if (!lexer.next(number_token, err))
    {
        return testing::AssertionFailure() << err.to_string() << mkerr();
    }

    jxc::util::NumberTokenSplitResult split;
    if (!jxc::util::split_number_token_value(number_token, split, err))
    {
        return testing::AssertionFailure() << err.to_string() << mkerr();
    }

    if (split.sign != expected_result.sign)
    {
        return testing::AssertionFailure() << jxc::format("Sign char {} != {}",
            jxc::detail::debug_char_repr(split.sign),
            jxc::detail::debug_char_repr(expected_result.sign)) << mkerr();
    }

    if (split.prefix != expected_result.prefix)
    {
        return testing::AssertionFailure() << jxc::format("Prefix {} != {}",
            jxc::detail::debug_string_repr(split.prefix),
            jxc::detail::debug_string_repr(expected_result.prefix)) << mkerr();
    }

    if (split.value != expected_result.value)
    {
        return testing::AssertionFailure() << jxc::format("Value {} != {}",
            jxc::detail::debug_string_repr(split.value),
            jxc::detail::debug_string_repr(expected_result.value)) << mkerr();
    }

    if (split.exponent != expected_result.exponent)
    {
        return testing::AssertionFailure() << jxc::format("Exponent {} != {}",
            split.exponent,
            expected_result.exponent) << mkerr();
    }

    if (split.suffix != expected_result.suffix)
    {
        return testing::AssertionFailure() << jxc::format("Suffix {} != {}",
            jxc::detail::debug_string_repr(split.suffix),
            jxc::detail::debug_string_repr(expected_result.suffix)) << mkerr();
    }

    if (split.float_type != expected_result.float_type)
    {
        return testing::AssertionFailure() << jxc::format("FloatType {} != {}",
            jxc::float_literal_type_to_string(split.float_type),
            jxc::float_literal_type_to_string(expected_result.float_type)) << mkerr();
    }

    jxc::Token dummy;
    if (lexer.next(dummy, err))
    {
        return testing::AssertionFailure() << "Expected end of token stream" << mkerr();
    }

    return testing::AssertionSuccess();
}

#define EXPECT_PARSE_NUMBER(JXC_STRING, SIGN, PREFIX, VALUE, EXPONENT, SUFFIX) \
    EXPECT_PRED_FORMAT2(test_parse_number, JXC_STRING, (jxc::util::NumberTokenSplitResult{ (SIGN), (PREFIX), (VALUE), (EXPONENT), (SUFFIX) }))

#define EXPECT_PARSE_FLOAT_LITERAL(JXC_STRING, SIGN, LITERAL) \
    EXPECT_PRED_FORMAT2(test_parse_number, JXC_STRING, (jxc::util::NumberTokenSplitResult{ (SIGN), "", "", 0, "", (LITERAL) }))


TEST(jxc_core, NumberParsing)
{
    using namespace jxc;

    EXPECT_PARSE_NUMBER("0", '+', "", "0", 0, "");
    EXPECT_PARSE_NUMBER("-1", '-', "", "1", 0, "");
    EXPECT_PARSE_NUMBER("1e20", '+', "", "1", 20, "");
    EXPECT_PARSE_NUMBER("-0x4f_px", '-', "0x", "4f", 0, "px");
    EXPECT_PARSE_NUMBER("0x0", '+', "0x", "0", 0, "");
    EXPECT_PARSE_NUMBER("0x1", '+', "0x", "1", 0, "");
    EXPECT_PARSE_NUMBER("0b1", '+', "0b", "1", 0, "");
    EXPECT_PARSE_NUMBER("0o0", '+', "0o", "0", 0, "");
    EXPECT_PARSE_NUMBER("0o567", '+', "0o", "567", 0, "");
    EXPECT_PARSE_NUMBER("-0o567px", '-', "0o", "567", 0, "px");
    EXPECT_PARSE_NUMBER("0b01101100%", '+', "0b", "01101100", 0, "%");

    EXPECT_PARSE_NUMBER("0.0", '+', "", "0.0", 0, "");
    EXPECT_PARSE_NUMBER("-0.0", '-', "", "0.0", 0, "");
    EXPECT_PARSE_NUMBER("-1.25555555555551f", '-', "", "1.25555555555551", 0, "f");

    EXPECT_PARSE_FLOAT_LITERAL("nan", '+', FloatLiteralType::NotANumber);
    EXPECT_PARSE_FLOAT_LITERAL("+inf", '+', FloatLiteralType::PosInfinity);
    EXPECT_PARSE_FLOAT_LITERAL("-inf", '-', FloatLiteralType::NegInfinity);
}


testing::AssertionResult test_parse_string_internal(const std::string& jxc_string, std::string& out_parsed_string)
{
    jxc::JumpParser parser(jxc_string);
    if (!parser.next())
    {
        return testing::AssertionFailure() << parser.get_error().to_string(jxc_string);
    }

    const jxc::Element& ele = parser.value();

    if (ele.type != jxc::ElementType::String)
    {
        return testing::AssertionFailure() << jxc::format("Expected String element, got {}", jxc::element_type_to_string(ele.type));
    }

    const jxc::Token& tok = ele.token;
    if (tok.type != jxc::TokenType::String)
    {
        return testing::AssertionFailure() << jxc::format("Expected String token, got {}", jxc::token_type_to_string(tok.type));
    }

    jxc::ErrorInfo err;

    if (!jxc::util::parse_string_token(tok, out_parsed_string, err))
    {
        return testing::AssertionFailure() << jxc::format("util::parse_string_token({}) failed: {}", tok.to_repr(), err.to_string(jxc_string));
    }

    return testing::AssertionSuccess();
}


testing::AssertionResult test_parse_string(const char* jxc_string_str, const char* expected_string_str,
    const std::string& jxc_string, const std::string& expected_string)
{
    std::string parsed_string;

    testing::AssertionResult result = test_parse_string_internal(jxc_string, parsed_string);
    if (!result)
    {
        return result;
    }

    if (parsed_string != expected_string)
    {
        return testing::AssertionFailure() << jxc::format("{} != {}",
            jxc::detail::debug_string_repr(expected_string, '`'),
            jxc::detail::debug_string_repr(parsed_string, '`'));
    }

    return testing::AssertionSuccess();
}


testing::AssertionResult test_parse_string_fail(const char* jxc_string_str, const std::string& jxc_string)
{
    std::string parsed_string;

    testing::AssertionResult result = test_parse_string_internal(jxc_string, parsed_string);
    if (result)
    {
        return testing::AssertionFailure()
            << jxc::format("Expected string {} to cause a parse failure, but it parsed successfully. ", jxc::detail::debug_string_repr(jxc_string))
            << result.message();
    }

    return testing::AssertionSuccess() << "String parse failure: " << result.message();
}


#define EXPECT_PARSE_STRING(JXC_STRING, EXPECTED_STRING) EXPECT_PRED_FORMAT2(test_parse_string, JXC_STRING, (EXPECTED_STRING))
#define EXPECT_PARSE_STRING_FAIL(JXC_STRING) EXPECT_PRED_FORMAT1(test_parse_string_fail, JXC_STRING)


TEST(jxc_core, StringParsing)
{
    EXPECT_PARSE_STRING("''", "");
    EXPECT_PARSE_STRING("'abc'", "abc");
    EXPECT_PARSE_STRING("'\\nabc\\n'", "\nabc\n");
}


TEST(jxc_core, RawStringParsing)
{
    EXPECT_PARSE_STRING(R"JXC(r'()')JXC", "");
    EXPECT_PARSE_STRING(R"JXC(r"()")JXC", "");
    EXPECT_PARSE_STRING(R"JXC(r"(abc)")JXC", "abc");
    EXPECT_PARSE_STRING(R"JXC(r"(\nabc\t)")JXC", "\\nabc\\t");
    EXPECT_PARSE_STRING(R"JXC(r'(

!abc!

)')JXC", "\n\n!abc!\n\n");
    EXPECT_PARSE_STRING("r'HEREDOC()HEREDOC'", "");
    EXPECT_PARSE_STRING("r'HEREDOC(abc)HEREDOC'", "abc");
    EXPECT_PARSE_STRING(R"JXC(r'HEREDOC(

!abc!

)HEREDOC')JXC", "\n\n!abc!\n\n");

    EXPECT_PARSE_STRING_FAIL("r''") << "missing parens";
    EXPECT_PARSE_STRING_FAIL("r')'") << "missing open paren";
    EXPECT_PARSE_STRING_FAIL("r'('") << "missing trailing paren";
    EXPECT_PARSE_STRING_FAIL("r'HEREDOC_111()HEREDOC_999'") << "heredoc does not match";
    EXPECT_PARSE_STRING_FAIL("r'HEREDOC_HEREDOC_HEREDOC()HEREDOC_HEREDOC_HEREDOC'") << "heredoc too long";
}


testing::AssertionResult test_parse_bytes(const char* jxc_string_str, const char* expected_bytes_str,
    const std::string& jxc_string, std::initializer_list<uint8_t> expected_bytes)
{
    jxc::JumpParser parser(jxc_string);
    if (!parser.next())
    {
        return testing::AssertionFailure() << parser.get_error().to_string(jxc_string);
    }

    const jxc::Element& ele = parser.value();

    if (ele.type != jxc::ElementType::Bytes)
    {
        return testing::AssertionFailure() << jxc::format("Expected Bytes element, got {}", jxc::element_type_to_string(ele.type));
    }

    const jxc::Token& tok = ele.token;
    if (tok.type != jxc::TokenType::ByteString)
    {
        return testing::AssertionFailure() << jxc::format("Expected ByteString token, got {}", jxc::token_type_to_string(tok.type));
    }

    std::vector<uint8_t> expected_data = expected_bytes;
    std::vector<uint8_t> parsed_data;

    jxc::ErrorInfo err;

    if (!jxc::util::parse_bytes_token(tok, parsed_data, err))
    {
        return testing::AssertionFailure() << jxc::format("util::parse_bytes_token({}) failed: {}", tok.to_repr(), err.to_string(jxc_string));
    }

    if (parsed_data.size() != expected_data.size())
    {
        return testing::AssertionFailure() << jxc::format("Expected {} bytes, got {}", expected_data.size(), parsed_data.size());
    }

    if (parsed_data != expected_data)
    {
        return testing::AssertionFailure() << jxc::format("{} != {}",
            jxc::detail::debug_bytes_repr(expected_data, '`'),
            jxc::detail::debug_bytes_repr(parsed_data, '`'));
    }

    return testing::AssertionSuccess();
}


#define EXPECT_PARSE_BYTES(JXC_STRING, ...) EXPECT_PRED_FORMAT2(test_parse_bytes, JXC_STRING, (std::initializer_list<uint8_t>{ __VA_ARGS__ }))


TEST(jxc_core, BytesParsing)
{
    EXPECT_PARSE_BYTES("b64''");
    EXPECT_PARSE_BYTES("b64'()'");
    EXPECT_PARSE_BYTES("b64'(                      )'");
    EXPECT_PARSE_BYTES("b64'(   \n\t       \n\t      )'");
    EXPECT_PARSE_BYTES("b64'AA=='", 0x00);
    EXPECT_PARSE_BYTES("b64'+g=='", 0xfa);
    EXPECT_PARSE_BYTES("b64'+gBKDw=='", 0xfa, 0x00, 0x4a, 0x0f);
    EXPECT_PARSE_BYTES("b64'anhj'", 'j', 'x', 'c');
    EXPECT_PARSE_BYTES("b64'(anhj)'", 'j', 'x', 'c');
    EXPECT_PARSE_BYTES("b64'( anhj )'", 'j', 'x', 'c');
    EXPECT_PARSE_BYTES("b64'(\n            anhj\n          \n)'", 'j', 'x', 'c');
    EXPECT_PARSE_BYTES("b64'(  \t      \na        \nn   \t      \nh   \t   \nj   \t\t\n)'", 'j', 'x', 'c');
    EXPECT_PARSE_BYTES("b64\"(\n  anhjIGZ\n  vcm1hdA==\n  )\"", 'j', 'x', 'c', ' ', 'f', 'o', 'r', 'm', 'a', 't');
}


TEST(jxc_core, DateToISO8601)
{
    using jxc::Date, jxc::date_to_iso8601;
    EXPECT_EQ(date_to_iso8601(Date()), "1970-01-01");
    EXPECT_EQ(date_to_iso8601(Date(1970, 1, 1)), "1970-01-01");
    EXPECT_EQ(date_to_iso8601(Date(-1, 1, 1)), "-0001-01-01");
    EXPECT_EQ(date_to_iso8601(Date(0, 1, 1)), "0000-01-01");
    EXPECT_EQ(date_to_iso8601(Date(1, 1, 1)), "0001-01-01");
    EXPECT_EQ(date_to_iso8601(Date(-10000, 1, 1)), "-10000-01-01");
    EXPECT_EQ(date_to_iso8601(Date(-10000, 12, 31)), "-10000-12-31");
    EXPECT_EQ(date_to_iso8601(Date(2012, 12, 12)), "2012-12-12");
    EXPECT_EQ(date_to_iso8601(Date(3012, 12, 12)), "3012-12-12");
    EXPECT_EQ(date_to_iso8601(Date(10000, 12, 12)), "10000-12-12");
}


TEST(jxc_core, DateTimeToISO8601)
{
    using jxc::DateTime, jxc::datetime_to_iso8601;
    EXPECT_EQ(datetime_to_iso8601(DateTime()), "1970-01-01T00:00:00Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(1970, 1, 1)), "1970-01-01T00:00:00Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(-1, 1, 1)), "-0001-01-01T00:00:00Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(0, 1, 1)), "0000-01-01T00:00:00Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(1, 1, 1, 1, 1, 1, 1)), "0001-01-01T01:01:01.000000001Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(-10000, 1, 1)), "-10000-01-01T00:00:00Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(-10000, 12, 31)), "-10000-12-31T00:00:00Z");
    EXPECT_EQ(datetime_to_iso8601(DateTime(2012, 12, 12)), "2012-12-12T00:00:00Z");

    // test auto-stripping the time component if it's all zeroes
    EXPECT_EQ(datetime_to_iso8601(DateTime(2012, 12, 12, 0, 0, 0, 0), true), "2012-12-12");
    EXPECT_EQ(datetime_to_iso8601(DateTime(1997, 02, 19, 0, 0, 0, 0), true), "1997-02-19");

    EXPECT_EQ(datetime_to_iso8601(DateTime::make_utc(2007, 11, 22, 12, 21, 49, ms_to_ns(332))), "2007-11-22T12:21:49.332Z");

    EXPECT_EQ(datetime_to_iso8601(DateTime::make_local(2000, 1, 1, 12, 35, 22, ms_to_ns(250))), "2000-01-01T12:35:22.250");
    EXPECT_EQ(datetime_to_iso8601(DateTime::make_local(2023, 2, 10, 8, 22, 59, ms_to_ns(777))), "2023-02-10T08:22:59.777");
    EXPECT_EQ(datetime_to_iso8601(DateTime::make_local(-50, 12, 31, 14, 49, 10, ms_to_ns(205))), "-0050-12-31T14:49:10.205");
}


testing::AssertionResult test_parse_datetime(const char* jxc_string_str, const char* expected_datetime_str,
    const std::string& jxc_string, const jxc::DateTime& expected_datetime)
{
    jxc::JumpParser parser(jxc_string);
    if (!parser.next())
    {
        return testing::AssertionFailure() << parser.get_error().to_string(jxc_string);
    }

    const jxc::Element& ele = parser.value();

    if (ele.type != jxc::ElementType::DateTime)
    {
        return testing::AssertionFailure() << jxc::format("Expected DateTime element, got {}", jxc::element_type_to_string(ele.type));
    }

    const jxc::Token& tok = ele.token;
    if (tok.type != jxc::TokenType::DateTime)
    {
        return testing::AssertionFailure() << jxc::format("Expected DateTime token, got {}", jxc::token_type_to_string(tok.type));
    }

    jxc::DateTime parsed_datetime;
    jxc::ErrorInfo err;
    if (!jxc::util::parse_datetime_token(tok, parsed_datetime, err))
    {
        return testing::AssertionFailure() << jxc::format("util::parse_datetime_token({}) failed: {}", tok.to_repr(), err.to_string(jxc_string));
    }

    if (parsed_datetime != expected_datetime)
    {
        return testing::AssertionFailure()
            << jxc::format("{} != {}",
                jxc::datetime_to_iso8601(expected_datetime),
                jxc::datetime_to_iso8601(parsed_datetime));
    }

    return testing::AssertionSuccess();
}


#define EXPECT_PARSE_DATETIME(JXC_STRING, DATETIME_VALUE) EXPECT_PRED_FORMAT2(test_parse_datetime, JXC_STRING, (DATETIME_VALUE))


TEST(jxc_core, DateTimeParsing)
{
    EXPECT_PARSE_DATETIME("dt'2023-06-15'", jxc::DateTime(2023, 6, 15));
    EXPECT_PARSE_DATETIME("dt'1600-01-01'", jxc::DateTime(1600, 1, 1));
    EXPECT_PARSE_DATETIME("dt'1600-12-31'", jxc::DateTime(1600, 12, 31));
    EXPECT_PARSE_DATETIME("dt'0000-01-01'", jxc::DateTime(0, 1, 1));
    EXPECT_PARSE_DATETIME("dt'-0001-01-01'", jxc::DateTime(-1, 1, 1));
    EXPECT_PARSE_DATETIME("dt'-2000-10-12'", jxc::DateTime(-2000, 10, 12));
    EXPECT_PARSE_DATETIME("dt'+4200-04-22'", jxc::DateTime(4200, 4, 22));

    // tests for fractional second to nanosecond conversion
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.383201024Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383201024));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.38320102Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383201020));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.3832017Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383201700));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.383201Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383201000));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.38320Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383200000));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.3832Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383200000));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.383Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383000000));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.38Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 380000000));
    EXPECT_PARSE_DATETIME("dt'2025-08-21T10:25:05.4Z'", jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 400000000));
}


TEST(jxc_core, SerializerSimple)
{
    using namespace jxc;

    // null
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_null(); }), "null");
    
    // bool
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bool(true); }), "true");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bool(false); }), "false");

    // signed integers (dec)
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int(0); }), "0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int(42); }), "42");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int(-42); }), "-42");

    // signed integers (bin)
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(0); }), "0b0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(1); }), "0b1");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(2); }), "0b10");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(3); }), "0b11");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(4); }), "0b100");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(-1); }), "-0b1");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(-2); }), "-0b10");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(-3); }), "-0b11");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_bin(-4); }), "-0b100");

    // signed integers (oct)
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(0); }), "0o0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(6); }), "0o6");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(7); }), "0o7");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(8); }), "0o10");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(9); }), "0o11");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(10); }), "0o12");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(-6); }), "-0o6");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(-7); }), "-0o7");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(-8); }), "-0o10");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(-9); }), "-0o11");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_oct(-10); }), "-0o12");

    // signed integers (hex)
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(0); }), "0x0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(1); }), "0x1");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(-1); }), "-0x1");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(15); }), "0xf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(16); }), "0x10");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(-15); }), "-0xf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(-16); }), "-0x10");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(255); }), "0xff");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_int_hex(-255); }), "-0xff");

    // unsigned ints
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_uint(0); }), "0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_uint(42); }), "42");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_uint_bin(42); }), "0b101010");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_uint_oct(42); }), "0o52");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_uint_hex(42); }), "0x2a");

    // floats
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(0); }), "0.0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(1); }), "1.0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(-1); }), "-1.0");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(1.5, {}, 4, false); }), "1.5");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(1.5, {}, 4, true); }), "1.5000");

    // float literals
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(std::numeric_limits<float>::quiet_NaN()); }), "nan");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(std::numeric_limits<float>::signaling_NaN()); }), "nan");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(std::numeric_limits<double>::quiet_NaN()); }), "nan");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(std::numeric_limits<double>::signaling_NaN()); }), "nan");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_nan(); }), "nan");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(std::numeric_limits<float>::infinity()); }), "+inf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(std::numeric_limits<double>::infinity()); }), "+inf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_pos_infinity(); }), "+inf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(-std::numeric_limits<float>::infinity()); }), "-inf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_float(-std::numeric_limits<double>::infinity()); }), "-inf");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_neg_infinity(); }), "-inf");

    // strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("", StringQuoteMode::Auto); }), "\"\"");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("", StringQuoteMode::Single); }), "''");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("", StringQuoteMode::Double); }), "\"\"");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("abc", StringQuoteMode::Single); }), "'abc'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("abc\ndef", StringQuoteMode::Single); }), "'abc\\ndef'");

    // raw strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("", StringQuoteMode::Single); }), "r'()'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("", StringQuoteMode::Single, "HEREDOC"); }), "r'HEREDOC()HEREDOC'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("abc\ndef", StringQuoteMode::Single); }), "r'(abc\ndef)'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("abc\ndef", StringQuoteMode::Single, "HEREDOC"); }), "r'HEREDOC(abc\ndef)HEREDOC'");

    // base64 strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_base64(BytesView(), StringQuoteMode::Single); }), "b64''");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_base64(BytesValue{ 'j','x','c',' ','f','o','r','m','a','t' }, StringQuoteMode::Single); }), "b64'anhjIGZvcm1hdA=='");

    // date
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_date(jxc::Date(1970, 1, 1), StringQuoteMode::Single); }), "dt'1970-01-01'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_date(jxc::Date(2025, 8, 21), StringQuoteMode::Single); }), "dt'2025-08-21'");

    // datetime
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_local(2025, 8, 21, 10, 25), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:00'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_local(2025, 8, 21, 10, 25, 5), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 1), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.000000001Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 100000000), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.100Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, 383201024), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.383201024Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, ms_to_ns(24)), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.024Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, ms_to_ns(602)), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.602Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, ms_to_ns(500)), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.500Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, us_to_ns(1)), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.000001Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, us_to_ns(123)), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.000123Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime::make_utc(2025, 8, 21, 10, 25, 5, us_to_ns(8765)), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05.008765Z'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_datetime(jxc::DateTime(2025, 8, 21, 10, 25, 5, 0, -8, 0), false, StringQuoteMode::Single); }), "dt'2025-08-21T10:25:05-08:00'");

    // arrays
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.array_begin().array_end(); }), "[]");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.array_begin().value_null().value_int(123).value_bool(true).array_end(); }), "[null,123,true]");

    // objects
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.object_begin().object_end(); }), "{}");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.object_begin().identifier("x").sep().value_int(0).object_end(); }), "{x:0}");

    // expressions
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.expression_begin().expression_end(); }), "()");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.expression_begin().value_int(1).op("+").value_int(2).expression_end(); }), "(1+2)");

    // annotations
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.annotation("list<int>").value_null(); }), "list<int> null");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.annotation("list<int>").array_empty(); }), "list<int>[]");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.annotation("vec3").array_begin().value_int(0).value_int(1).value_int(2).array_end(); }), "vec3[0,1,2]");
    EXPECT_EQ(test_serialize([](Serializer& doc) {
        doc.annotation("vec3")
        .object_begin()
        .identifier("x").sep().value_int(0)
        .identifier("y").sep().value_int(1)
        .identifier("z").sep().value_int(2)
        .object_end(); }),
        "vec3{x:0,y:1,z:2}");

}
