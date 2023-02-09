#pragma once
#include "gtest/gtest.h"
#include "jxc_tests.h"
#include "jxc/jxc.h"


// time conversion helpers useful for the date/datetime tests
inline uint32_t ms_to_ns(uint32_t value_ms) { return value_ms * static_cast<uint32_t>(1e6); }
inline uint32_t us_to_ns(uint32_t value_us) { return value_us * static_cast<uint32_t>(1e3); }


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
    else if (lhs.annotation != rhs.annotation)
    {
        return testing::AssertionFailure() << "Element annotation mismatch: " << lhs.annotation.to_repr() << " != " << rhs.annotation.to_repr()
                                           << " (`" << lhs.annotation.to_string() << "` != `" << rhs.annotation.to_string() << "`)";
    }

    return testing::AssertionFailure() << lhs.to_repr() << " != " << rhs.to_repr();
}


inline jxc::Token make_token(jxc::TokenType tok_type, std::string_view tok_value = std::string_view{})
{
    return jxc::Token(tok_type, jxc::invalid_idx, jxc::invalid_idx, tok_value);
}


inline jxc::Token make_token(char symbol)
{
    jxc::TokenType tok_type = jxc::token_type_from_symbol(symbol);
    JXC_ASSERT(tok_type != jxc::TokenType::Invalid);
    JXC_ASSERT(!jxc::token_type_has_value(tok_type));
    return jxc::Token(tok_type);
}


inline jxc::Token make_token(std::string_view symbol)
{
    jxc::TokenType tok_type = jxc::token_type_from_symbol(symbol);
    JXC_ASSERT(tok_type != jxc::TokenType::Invalid);
    JXC_ASSERT(!jxc::token_type_has_value(tok_type));
    return jxc::Token(tok_type, jxc::invalid_idx, jxc::invalid_idx, symbol);
}


inline jxc::OwnedElement make_element(jxc::ElementType ele_type, const jxc::Token& token, std::initializer_list<jxc::Token> annotation_tokens = {})
{
    jxc::OwnedTokenSpan anno;
    for (const jxc::Token& tok: annotation_tokens)
    {
        anno.tokens.push(tok);
    }
    return jxc::OwnedElement(ele_type, token, anno);
}


struct BytesValue
{
    std::vector<uint8_t> data;

    BytesValue(std::initializer_list<uint8_t> values)
        : data(values)
    {
    }

    inline operator jxc::BytesView() const
    {
        return jxc::BytesView(data.data(), data.size());
    }
};
