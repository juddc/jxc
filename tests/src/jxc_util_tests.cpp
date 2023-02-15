#include "jxc_util_tests.h"


TEST(jxc_format, Format)
{
    // empty
    EXPECT_EQ(jxc::format(""), "");

    // constant
    EXPECT_EQ(jxc::format("abc def ghi"), "abc def ghi");

    // null
    EXPECT_EQ(jxc::format("{}", nullptr), "0x0");

    // bool
    EXPECT_EQ(jxc::format("{}", false), "false");
    EXPECT_EQ(jxc::format("{}", true), "true");

    // integers
    EXPECT_EQ(jxc::format("{}", static_cast<uint8_t>(0u)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<uint8_t>(255u)), "255");

    EXPECT_EQ(jxc::format("{}", static_cast<uint16_t>(0u)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<uint16_t>(65535u)), "65535");

    EXPECT_EQ(jxc::format("{}", static_cast<uint32_t>(0ul)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<uint32_t>(4294967295ul)), "4294967295");

    EXPECT_EQ(jxc::format("{}", static_cast<uint64_t>(0ull)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<uint64_t>(18446744073709551615Ull)), "18446744073709551615");

    EXPECT_EQ(jxc::format("{}", static_cast<int8_t>(0)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<int8_t>(-128)), "-128");
    EXPECT_EQ(jxc::format("{}", static_cast<int8_t>(127)), "127");

    EXPECT_EQ(jxc::format("{}", static_cast<int16_t>(0)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<int16_t>(-32768)), "-32768");
    EXPECT_EQ(jxc::format("{}", static_cast<int16_t>(32767)), "32767");

    EXPECT_EQ(jxc::format("{}", static_cast<int32_t>(0)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<int32_t>(-2147483648)), "-2147483648");
    EXPECT_EQ(jxc::format("{}", static_cast<int32_t>(2147483647)), "2147483647");

    EXPECT_EQ(jxc::format("{}", static_cast<int64_t>(0ll)), "0");
    EXPECT_EQ(jxc::format("{}", static_cast<int64_t>(-9223372036854775807ll)), "-9223372036854775807");
    EXPECT_EQ(jxc::format("{}", static_cast<int64_t>(9223372036854775807ll)), "9223372036854775807");

    // hex integers
    EXPECT_EQ(jxc::format("{:x}", static_cast<uint8_t>(255)), "ff");
    EXPECT_NE(jxc::format("{:x}", static_cast<uint8_t>(255)), "8a");
    const uint8_t val = static_cast<uint8_t>(0xfa);
    EXPECT_EQ(jxc::format("{:x}", val), "fa");
    EXPECT_EQ(jxc::format("{:x}", 4444), "115c");
    EXPECT_EQ(jxc::format("{:x}", 0), "0");

    // octal integers
    EXPECT_EQ(jxc::format("{:o}", 0), "0");
    EXPECT_EQ(jxc::format("{:o}", 511), "777");

    // binary integers
    EXPECT_EQ(jxc::format("{:b}", 0), "0");
    EXPECT_EQ(jxc::format("{:b}", 1), "1");
    EXPECT_EQ(jxc::format("{:b}", 2), "10");
    EXPECT_EQ(jxc::format("{:b}", 3), "11");
    EXPECT_EQ(jxc::format("{:b}", 4), "100");
    EXPECT_EQ(jxc::format("{:b}", 5), "101");
    EXPECT_EQ(jxc::format("{:b}", 6), "110");
    EXPECT_EQ(jxc::format("{:b}", 7), "111");
    EXPECT_EQ(jxc::format("{:b}", 8), "1000");

    // floats (without precision)
    EXPECT_EQ(jxc::format("{}", 0.0f), "0");
    EXPECT_EQ(jxc::format("{}", 0.5f), "0.5");
    EXPECT_EQ(jxc::format("{}", -0.5f), "-0.5");

    // floats (with formatted fixed precision)
    EXPECT_EQ(jxc::format("{:.4f}", 0.000341f), "0.0003");
    EXPECT_EQ(jxc::format("{:.2f}", 0.5234f), "0.52");
    EXPECT_EQ(jxc::format("{:.6f}", -0.5441234321f), "-0.544123");

    // floats (with dynamic fixed precision)
    EXPECT_EQ(jxc::format("{}", jxc::FloatPrecision(0.000341f, 4)), "0.0003");
    EXPECT_EQ(jxc::format("{}", jxc::FloatPrecision(0.5234f, 2)), "0.52");
    EXPECT_EQ(jxc::format("{}", jxc::FloatPrecision(-0.5441234321f, 6)), "-0.544123");

    // doubles (without precision)
    EXPECT_EQ(jxc::format("{}", 0.0), "0");
    EXPECT_EQ(jxc::format("{}", 0.5), "0.5");
    EXPECT_EQ(jxc::format("{}", -0.5), "-0.5");

    // doubles (with formatted fixed precision)
    EXPECT_EQ(jxc::format("{:.4f}", 0.000341), "0.0003");
    EXPECT_EQ(jxc::format("{:.2f}", 0.5234), "0.52");
    EXPECT_EQ(jxc::format("{:.6f}", -0.5441234321), "-0.544123");

    // doubles (with dynamic fixed precision)
    EXPECT_EQ(jxc::format("{}", jxc::FloatPrecision(0.000341, 4)), "0.0003");
    EXPECT_EQ(jxc::format("{}", jxc::FloatPrecision(0.5234, 2)), "0.52");
    EXPECT_EQ(jxc::format("{}", jxc::FloatPrecision(-0.5441234321, 6)), "-0.544123");

    // char
    EXPECT_EQ(jxc::format("{}", 'c'), "c");
    EXPECT_EQ(jxc::format("{}", '~'), "~");

    // string literals
    EXPECT_EQ(jxc::format("{}", ""), "");
    EXPECT_EQ(jxc::format("{}", "a"), "a");
    EXPECT_EQ(jxc::format("{}", "abc 123 ghi"), "abc 123 ghi");

    // std::string_view
    EXPECT_EQ(jxc::format("{}", ""), std::string_view(""));
    EXPECT_EQ(jxc::format("{}", "a"), std::string_view("a"));
    EXPECT_EQ(jxc::format("{}", "abc 123 ghi"), std::string_view("abc 123 ghi"));

    // std::string
    EXPECT_EQ(jxc::format("{}", ""), std::string(""));
    EXPECT_EQ(jxc::format("{}", "a"), std::string("a"));
    EXPECT_EQ(jxc::format("{}", "abc 123 ghi"), std::string("abc 123 ghi"));

    // const char*
    const char* str_a = ""; EXPECT_EQ(jxc::format("{}", str_a), str_a);
    const char* str_b = "a"; EXPECT_EQ(jxc::format("{}", str_b), str_b);
    const char* str_c = "abc 123 ghi"; EXPECT_EQ(jxc::format("{}", str_c), str_c);

    // left string
    EXPECT_EQ(jxc::format("abc_{}", 123), "abc_123");

    // right string
    EXPECT_EQ(jxc::format("{}_cba", 123), "123_cba");

    // both sides
    EXPECT_EQ(jxc::format("abc_{}_cba", 123), "abc_123_cba");

    // multiple values
    EXPECT_EQ(jxc::format("abc_{}_{}_cba", 123, 456), "abc_123_456_cba");
    EXPECT_EQ(jxc::format("abc_{}_{}_{}_cba", 123, 456, 789), "abc_123_456_789_cba");

    // multiple values (mixed types)
    EXPECT_EQ(jxc::format("abc_{}_{}_{}_cba", 0.5, nullptr, true), "abc_0.5_0x0_true_cba");

    // 16 args
    EXPECT_EQ(jxc::format("{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}",
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
        "0.1.2.3.4.5.6.7.8.9.10.11.12.13.14.15");
}


TEST(jxc_util, StringViewHelpers)
{
    std::string_view view;

    // uninitialized view
    EXPECT_FALSE(jxc::detail::string_view_starts_with(view, "!"));
    EXPECT_FALSE(jxc::detail::string_view_ends_with(view, "!"));

    // empty view
    view = "";
    EXPECT_FALSE(jxc::detail::string_view_starts_with(view, "!"));
    EXPECT_FALSE(jxc::detail::string_view_ends_with(view, "!"));

    // length-1 view
    view = "a";
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, 'a'));
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, "a"));
    EXPECT_FALSE(jxc::detail::string_view_starts_with(view, "7"));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, 'a'));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, "a"));
    EXPECT_FALSE(jxc::detail::string_view_ends_with(view, "7"));

    // longer view
    view = "abc-def-ghi";
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, 'a'));
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, ""));
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, "a"));
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, "abc"));
    EXPECT_TRUE(jxc::detail::string_view_starts_with(view, "abc-"));
    EXPECT_FALSE(jxc::detail::string_view_starts_with(view, "J"));
    EXPECT_FALSE(jxc::detail::string_view_starts_with(view, ".!@"));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, 'i'));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, ""));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, "i"));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, "hi"));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, "ghi"));
    EXPECT_TRUE(jxc::detail::string_view_ends_with(view, "-ghi"));
    EXPECT_FALSE(jxc::detail::string_view_ends_with(view, "Z"));
    EXPECT_FALSE(jxc::detail::string_view_ends_with(view, "ABC"));
}


TEST(jxc_util, TokenTypeMetadata)
{
    using namespace jxc;
    for (uint8_t i = 0; i < static_cast<uint8_t>(TokenType::COUNT); i++)
    {
        const TokenType tok_type = static_cast<TokenType>(i);
        std::string_view tok_name = token_type_to_string(tok_type);
        std::string_view tok_symbol = token_type_to_symbol(tok_type);

        EXPECT_GT(tok_name.size(), 0) << "TokenType(" << static_cast<size_t>(i) << ") should have a defined name";

        // a TokenType should have a value or a symbol, but not both
        if (token_type_has_value(tok_type))
        {
            EXPECT_EQ(tok_symbol.size(), 0) << "TokenType::" << tok_name << " has a value, so it should not have a symbol";
        }
        else if (tok_type != TokenType::Invalid && tok_type != TokenType::EndOfStream)
        {
            EXPECT_GT(tok_symbol.size(), 0) << "TokenType::" << tok_name << " has no value, so it should have a symbol";

            const TokenType tok_type_rev_symbol_lookup = token_type_from_symbol(tok_symbol);
            EXPECT_EQ(tok_type_rev_symbol_lookup, tok_type)
                << "token_type_from_symbol(token_type_to_symbol(" << tok_name << ")) should equal " << tok_name;

            // also test the token_type_from_symbol(char) overload
            if (tok_symbol.size() == 1)
            {
                const char tok_symbol_char = tok_symbol[0];
                const TokenType tok_type_rev_symbol_lookup = token_type_from_symbol(tok_symbol_char);
                EXPECT_EQ(tok_type_rev_symbol_lookup, tok_type)
                    << "token_type_from_symbol(token_type_to_symbol((char)'" << tok_symbol_char << "')) should equal " << tok_name;
            }
        }
    }
}


TEST(jxc_util, FloatLiteralTypes)
{
    // finite
    EXPECT_EQ(jxc::get_float_literal_type<float>(0.0f), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<float>(12345.0f), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<float>(-12345.0f), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<float>(1e-2f), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<float>(-2.0133e4f), jxc::FloatLiteralType::Finite);

    EXPECT_EQ(jxc::get_float_literal_type<double>(0.0), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<double>(0.00000001), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<double>(10000000009.00002), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<double>(-99999999.123456789), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<double>(1e6), jxc::FloatLiteralType::Finite);
    EXPECT_EQ(jxc::get_float_literal_type<double>(-7.9234344e-8), jxc::FloatLiteralType::Finite);

    // nan
    EXPECT_EQ(jxc::get_float_literal_type<float>(sqrtf(-1.0f)), jxc::FloatLiteralType::NotANumber);
    EXPECT_EQ(jxc::get_float_literal_type<float>(std::numeric_limits<float>::quiet_NaN()), jxc::FloatLiteralType::NotANumber);
    EXPECT_EQ(jxc::get_float_literal_type<float>(std::numeric_limits<float>::signaling_NaN()), jxc::FloatLiteralType::NotANumber);

    EXPECT_EQ(jxc::get_float_literal_type<double>(sqrt(-1.0)), jxc::FloatLiteralType::NotANumber);
    EXPECT_EQ(jxc::get_float_literal_type<double>(std::numeric_limits<double>::quiet_NaN()), jxc::FloatLiteralType::NotANumber);
    EXPECT_EQ(jxc::get_float_literal_type<double>(std::numeric_limits<double>::signaling_NaN()), jxc::FloatLiteralType::NotANumber);

    // positive infinity
#if !defined(_MSC_VER)
    EXPECT_EQ(jxc::get_float_literal_type<float>(1.0f / 0.0f), jxc::FloatLiteralType::PosInfinity);
#endif
    EXPECT_EQ(jxc::get_float_literal_type<float>(std::numeric_limits<float>::infinity()), jxc::FloatLiteralType::PosInfinity);

#if !defined(_MSC_VER)
    EXPECT_EQ(jxc::get_float_literal_type<double>(1.0 / 0.0), jxc::FloatLiteralType::PosInfinity);
#endif
    EXPECT_EQ(jxc::get_float_literal_type<double>(std::numeric_limits<double>::infinity()), jxc::FloatLiteralType::PosInfinity);

    // negative infinity
#if !defined(_MSC_VER)
    EXPECT_EQ(jxc::get_float_literal_type<float>(-1.0f / 0.0f), jxc::FloatLiteralType::NegInfinity);
#endif
    EXPECT_EQ(jxc::get_float_literal_type<float>(-std::numeric_limits<float>::infinity()), jxc::FloatLiteralType::NegInfinity);

#if !defined(_MSC_VER)
    EXPECT_EQ(jxc::get_float_literal_type<double>(-1.0 / 0.0), jxc::FloatLiteralType::NegInfinity);
#endif
    EXPECT_EQ(jxc::get_float_literal_type<double>(-std::numeric_limits<double>::infinity()), jxc::FloatLiteralType::NegInfinity);
}
