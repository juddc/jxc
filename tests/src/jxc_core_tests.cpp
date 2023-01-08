#include "jxc_core_tests.h"


template<typename LhsT, typename RhsT>
testing::AssertionResult test_elements_equal(const LhsT& lhs, const RhsT& rhs)
{
    static_assert(std::is_same_v<LhsT, jxc::Element> || std::is_same_v<LhsT, jxc::OwnedElement>);
    static_assert(std::is_same_v<RhsT, jxc::Element> || std::is_same_v<RhsT, jxc::OwnedElement>);
    if (lhs == rhs)
    {
        return testing::AssertionSuccess();
    }

    if (lhs.type != rhs.type)
    {
        return testing::AssertionFailure() << "Element type mismatch: " << jxc::element_type_to_string(lhs.type) << " != " << jxc::element_type_to_string(rhs.type);
    }
    else if (lhs.token.type != rhs.token.type)
    {
        return testing::AssertionFailure() << "Element token type mismatch: " << jxc::token_type_to_string(lhs.token.type) << " != " << jxc::token_type_to_string(rhs.token.type);
    }
    else if (jxc::token_type_has_value(lhs.token.type) && lhs.token.value != rhs.token.value)
    {
        return testing::AssertionFailure() << "Element token value mismatch: " << jxc::detail::debug_string_repr(lhs.token.value.as_view()) << " != " << jxc::detail::debug_string_repr(rhs.token.value.as_view());
    }

    return testing::AssertionFailure() << lhs.to_repr() << " != " << rhs.to_repr();
}


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

        if (parser->next())
        {
            ++element_index;

            const Element& parsed_element = parser->value();
            if (parsed_element.type == ElementType::Invalid && parser->has_error())
            {
                done = true;
                return testing::AssertionFailure() << "Parse error: " << parser->get_error().to_string(jxc_string);
            }

            auto result = test_elements_equal(parsed_element, expected_element);
            if (!result)
            {
                done = true;
                return result;
            }
            else
            {
                return testing::AssertionSuccess();
            }
        }
        else
        {
            if (parser->has_error())
            {
                done = true;
                return testing::AssertionFailure() << "Parse error: " << parser->get_error().to_string(jxc_string);
            }
            else
            {
                done = true;
                if (expected_element.type == jxc::ElementType::Invalid)
                {
                    return testing::AssertionSuccess();
                }
                else
                {
                    return testing::AssertionFailure() << "Expected element " << expected_element.to_repr() << ", but parser is already done!";
                }
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


jxc::Token make_token(jxc::TokenType tok_type, std::string_view tok_value = std::string_view{})
{
    return jxc::Token(tok_type, jxc::invalid_idx, jxc::invalid_idx, tok_value);
}

jxc::Token make_token(char symbol)
{
    jxc::TokenType tok_type = jxc::token_type_from_symbol(symbol);
    JXC_ASSERT(tok_type != jxc::TokenType::Invalid);
    JXC_ASSERT(!jxc::token_type_has_value(tok_type));
    return jxc::Token(tok_type);
}

jxc::Token make_token(std::string_view symbol)
{
    jxc::TokenType tok_type = jxc::token_type_from_symbol(symbol);
    JXC_ASSERT(tok_type != jxc::TokenType::Invalid);
    JXC_ASSERT(!jxc::token_type_has_value(tok_type));
    return jxc::Token(tok_type, jxc::invalid_idx, jxc::invalid_idx, symbol);
}


jxc::OwnedElement make_element(jxc::ElementType ele_type, const jxc::Token& token, std::initializer_list<jxc::Token> annotation_tokens = {})
{
    jxc::OwnedTokenSpan anno;
    for (const jxc::Token& tok : annotation_tokens)
    {
        anno.tokens.push(tok);
    }
    return jxc::OwnedElement(ele_type, token, anno);
}


TEST(jxc_core, JumpParser)
{

    {
        TestJumpParser parser("null");
        EXPECT_PARSE_NEXT(parser, make_element(jxc::ElementType::Null, make_token(jxc::TokenType::Null)));
    }

}


TEST(jxc_core, Dummy)
{
    EXPECT_EQ(1 + 1, 2);
}

