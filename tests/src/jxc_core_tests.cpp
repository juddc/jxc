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

    // hexbyte strings
    EXPECT_PARSE_SINGLE("bx\"\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx\"\"")));
    EXPECT_PARSE_SINGLE("bx''", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx''")));
    EXPECT_PARSE_SINGLE("bx\"00acff\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx\"00acff\"")));
    EXPECT_PARSE_SINGLE("bx'00acff'", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx'00acff'")));

    // hexbyte strings (with parens - allows whitespace)
    EXPECT_PARSE_SINGLE("bx\"()\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx\"()\"")));
    EXPECT_PARSE_SINGLE("bx'()'", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx'()'")));
    EXPECT_PARSE_SINGLE("bx\"( 00 ac ff )\"", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx\"( 00 ac ff )\"")));
    EXPECT_PARSE_SINGLE("bx'( 00 ac ff )'", make_element(ElementType::Bytes, make_token(TokenType::ByteString, "bx'( 00 ac ff )'")));

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
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(true | false)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bool, make_token(TokenType::True)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "|")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Bool, make_token(TokenType::False)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndExpression, make_token(TokenType::ParenClose)));
    }

    {
        TestJumpParser parser("(abc=def, qqq=(1 + 2 / 3 * 4), zzz~=5)");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginExpression, make_token(TokenType::ParenOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionIdentifier, make_token(TokenType::Identifier, "abc")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "=")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionIdentifier, make_token(TokenType::Identifier, "def")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Comma, ",")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionIdentifier, make_token(TokenType::Identifier, "qqq")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "=")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::ParenOpen, "(")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "1")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "+")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "2")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "/")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "3")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "*")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "4")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::ParenClose, ")")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionToken, make_token(TokenType::Comma, ",")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionIdentifier, make_token(TokenType::Identifier, "zzz")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "~")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ExpressionOperator, make_token(TokenType::ExpressionOperator, "=")));
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
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::ObjectKeyIdentifier, "x")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::EndObject, make_token(TokenType::BraceClose)));
    }

    {
        TestJumpParser parser("{ x: 4.0, y: 0.0, z: -3.5 }");
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::BeginObject, make_token(TokenType::BraceOpen)));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::ObjectKeyIdentifier, "x")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "4.0")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::ObjectKeyIdentifier, "y")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::Number, make_token(TokenType::Number, "0.0")));
        EXPECT_PARSE_NEXT(parser, make_element(ElementType::ObjectKey, make_token(TokenType::ObjectKeyIdentifier, "z")));
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

    // strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("", StringQuoteMode::Auto); }), "\"\"");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("", StringQuoteMode::Single); }), "''");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("", StringQuoteMode::Double); }), "\"\"");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("abc", StringQuoteMode::Single); }), "'abc'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string("abc\ndef", StringQuoteMode::Single); }), "'abc\\ndef'");

    /// raw strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("", StringQuoteMode::Single); }), "r'()'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("", StringQuoteMode::Single, "HEREDOC"); }), "r'HEREDOC()HEREDOC'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("abc\ndef", StringQuoteMode::Single); }), "r'(abc\ndef)'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_string_raw("abc\ndef", StringQuoteMode::Single, "HEREDOC"); }), "r'HEREDOC(abc\ndef)HEREDOC'");

    // hexbyte strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_hex(BytesView(), StringQuoteMode::Single); }), "bx''");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_hex(BytesValue({ 0x0, 0xD, 0x7C }), StringQuoteMode::Single); }), "bx'000d7c'");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_hex(BytesValue({ 0x0, 0xD, 0x7C, 0x4F }), StringQuoteMode::Single); }), "bx'000d7c4f'");

    // base64 strings
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_base64(BytesView(), StringQuoteMode::Single); }), "b64''");
    EXPECT_EQ(test_serialize([](Serializer& doc) { doc.value_bytes_base64(BytesValue{ 'j','x','c',' ','f','o','r','m','a','t' }, StringQuoteMode::Single); }), "b64'anhjIGZvcm1hdA=='");

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
