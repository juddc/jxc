#include "jxc_converter_tests.h"


namespace fs = std::filesystem;


TEST(jxc_cpp_converter, ConverterCppTypeToAnnotation)
{
    using jxc::detail::cpp_type_to_annotation;

    EXPECT_EQ(
        cpp_type_to_annotation(
            "std::vector<int32_t>"),
        jxc::OwnedTokenSpan::parse_annotation(
            "std.vector<int32_t>"));

    EXPECT_EQ(
        cpp_type_to_annotation(
            "std::array<std::string,16>"),
        jxc::OwnedTokenSpan::parse_annotation(
            "std.array<std.string, 16>"));

    EXPECT_EQ(cpp_type_to_annotation(
            "std::conditional_t<value_type_is_trivial, ValueType, const ValueType&>"),
        jxc::OwnedTokenSpan::parse_annotation(
            "std.conditional_t<value_type_is_trivial, ValueType, const ValueType&>"));

    EXPECT_EQ(cpp_type_to_annotation(
            "::jxc::detail::FixedArray<::jxc::detail::FixedArray<int32_t>>"),
        jxc::OwnedTokenSpan::parse_annotation(
            "jxc.detail.FixedArray<jxc.detail.FixedArray<int32_t>>"));

    EXPECT_EQ(cpp_type_to_annotation(
            "std::vector<std::optional<std::vector<int32_t>>>"),
        jxc::OwnedTokenSpan::parse_annotation(
            "std.vector<std.optional<std.vector<int32_t>>>"));
}


// minimal, no-whitespace serializer settings for ease of test string comparisons
static const jxc::SerializerSettings settings_minimal = {
    .pretty_print = false,
    .target_line_length = 0,
    .indent = "",
    .linebreak = "",
    .key_separator = ":",
    .value_separator = ",",
};


template<typename T>
testing::AssertionResult _jxc_expect_converter_parse(
    const char* jxc_value_str, const char* expected_value_str,
    const std::string& jxc_value, const T& expected_value)
{
    auto result = jxc::conv::parse<T>(jxc_value);
    if (result == expected_value)
    {
        return testing::AssertionSuccess() << jxc_value_str << " == " << expected_value_str;
    }
    else
    {
        return testing::AssertionFailure() << jxc_value_str << " != " << expected_value_str;
    }
}


#define EXPECT_CONV_PARSE_EQ(TYPE, JXC_VALUE, EXPECTED_VALUE) EXPECT_PRED_FORMAT2(_jxc_expect_converter_parse<TYPE>, (JXC_VALUE), (EXPECTED_VALUE))
#define EXPECT_CONV_PARSE_NE(TYPE, JXC_VALUE, EXPECTED_VALUE) EXPECT_PRED_FORMAT2(!_jxc_expect_converter_parse<TYPE>, (JXC_VALUE), (EXPECTED_VALUE))

#define EXPECT_CONV_PARSE_BYTES_EQ(TYPE, JXC_VALUE, ...) EXPECT_PRED_FORMAT2(_jxc_expect_converter_parse<TYPE>, (JXC_VALUE), (TYPE{ __VA_ARGS__ }))
#define EXPECT_CONV_PARSE_BYTES_NE(TYPE, JXC_VALUE, ...) EXPECT_PRED_FORMAT2(!_jxc_expect_converter_parse<TYPE>, (JXC_VALUE), (TYPE{ __VA_ARGS__ }))


template<typename T>
testing::AssertionResult _jxc_expect_converter_serialize(
    const char* value_str, const char* expected_jxc_value_str,
    const T& value, const std::string& expected_jxc_value)
{
    const std::string result = jxc::conv::serialize(value, settings_minimal);
    if (result == expected_jxc_value)
    {
        return testing::AssertionSuccess() << jxc::detail::debug_string_repr(result) << " == " << expected_jxc_value_str;
    }
    else
    {
        return testing::AssertionFailure() << jxc::detail::debug_string_repr(result) << " != " << expected_jxc_value_str;
    }
}


#define EXPECT_CONV_SERIALIZE_EQ(VALUE, EXPECTED_JXC_STRING) EXPECT_PRED_FORMAT2(_jxc_expect_converter_serialize, (VALUE), (EXPECTED_JXC_STRING))
#define EXPECT_CONV_SERIALIZE_NE(VALUE, EXPECTED_JXC_STRING) EXPECT_PRED_FORMAT2(!_jxc_expect_converter_serialize, (VALUE), (EXPECTED_JXC_STRING))


TEST(jxc_cpp_converter, ConverterNullAndBool)
{
    EXPECT_CONV_PARSE_EQ(std::nullptr_t, "null", nullptr);
    EXPECT_CONV_PARSE_EQ(bool, "true", true);
    EXPECT_CONV_PARSE_EQ(bool, "false", false);

    EXPECT_CONV_SERIALIZE_EQ(nullptr, "null");
    EXPECT_CONV_SERIALIZE_NE(nullptr, "0");
    EXPECT_CONV_SERIALIZE_EQ(true, "true");
    EXPECT_CONV_SERIALIZE_EQ(false, "false");
}


TEST(jxc_cpp_converter, ConverterParseIntegers)
{
    EXPECT_CONV_PARSE_EQ(int8_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(int8_t, "-1", -1);
    EXPECT_CONV_PARSE_EQ(int8_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(int8_t, "127", INT8_MAX);
    EXPECT_CONV_PARSE_EQ(int8_t, "-128", INT8_MIN);
    EXPECT_THROW(jxc::conv::parse<int8_t>("-129"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<int8_t>("128"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<int8_t>("999"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(int16_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(int16_t, "-1", -1);
    EXPECT_CONV_PARSE_EQ(int16_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(int16_t, "32767", INT16_MAX);
    EXPECT_CONV_PARSE_EQ(int16_t, "-32768", INT16_MIN);
    EXPECT_THROW(jxc::conv::parse<int16_t>("32768"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<int16_t>("-32769"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(int32_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(int32_t, "-1", -1);
    EXPECT_CONV_PARSE_EQ(int32_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(int32_t, "12345", 12345);
    EXPECT_CONV_PARSE_NE(int32_t, "12345", 99999);
    EXPECT_CONV_PARSE_EQ(int32_t, "-99129945", -99129945);
    EXPECT_CONV_PARSE_EQ(int32_t, "2147483647", INT32_MAX);
    EXPECT_CONV_PARSE_EQ(int32_t, "-2147483648", INT32_MIN);
    EXPECT_THROW(jxc::conv::parse<int32_t>("2147483648"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<int32_t>("-2147483649"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(int64_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(int64_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(int64_t, "-1", -1);
    EXPECT_CONV_PARSE_EQ(int64_t, "9223372036854775807", INT64_MAX);

    // there's an edge case on INT64_MIN, so we want plenty of tests for values near it
    EXPECT_CONV_PARSE_EQ(int64_t, "-9223372036854775805", -9223372036854775805);
    EXPECT_CONV_PARSE_EQ(int64_t, "-9223372036854775806", -9223372036854775806);
    EXPECT_CONV_PARSE_EQ(int64_t, "-9223372036854775807", -9223372036854775807);
    EXPECT_CONV_PARSE_EQ(int64_t, "-9223372036854775808", INT64_MIN);

    EXPECT_THROW(jxc::conv::parse<int64_t>("9223372036854775808"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<int64_t>("-9223372036854775809"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(uint8_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(uint8_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(uint8_t, "254", 254);
    EXPECT_CONV_PARSE_EQ(uint8_t, "255", UINT8_MAX);
    EXPECT_THROW(jxc::conv::parse<uint8_t>("-1"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<uint8_t>("256"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<uint8_t>("999"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(uint16_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(uint16_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(uint16_t, "65535", UINT16_MAX);
    EXPECT_THROW(jxc::conv::parse<uint16_t>("-1"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<uint16_t>("65536"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(uint32_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(uint32_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(uint32_t, "12345678", 12345678);
    EXPECT_CONV_PARSE_EQ(uint32_t, "4294967295", UINT32_MAX);
    EXPECT_THROW(jxc::conv::parse<uint32_t>("-1"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<uint32_t>("4294967296"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(uint64_t, "0", 0);
    EXPECT_CONV_PARSE_EQ(uint64_t, "1", 1);
    EXPECT_CONV_PARSE_EQ(uint64_t, "18446744073709551615", UINT64_MAX);
    EXPECT_THROW(jxc::conv::parse<uint64_t>("-1"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<uint64_t>("18446744073709551616"), jxc::parse_error);
}


TEST(jxc_cpp_converter, ConverterSerializeIntegers)
{
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int8_t>(INT8_MIN), "-128");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int8_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int8_t>(INT8_MAX), "127");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<int16_t>(INT16_MIN), "-32768");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int16_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int16_t>(INT16_MAX), "32767");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<int32_t>(INT32_MIN), "-2147483648");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int32_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int32_t>(INT32_MAX), "2147483647");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<int64_t>(INT64_MIN), "-9223372036854775808");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int64_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<int64_t>(INT64_MAX), "9223372036854775807");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint8_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint8_t>(UINT8_MAX), "255");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint16_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint16_t>(UINT16_MAX), "65535");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint32_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint32_t>(UINT32_MAX), "4294967295");

    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint64_t>(0), "0");
    EXPECT_CONV_SERIALIZE_EQ(static_cast<uint64_t>(UINT64_MAX), "18446744073709551615");
}


TEST(jxc_cpp_converter, ConverterParseFloats)
{
    EXPECT_CONV_PARSE_EQ(float, "0", 0.0f);
    EXPECT_CONV_PARSE_EQ(float, "0.0", 0.0f);
    EXPECT_CONV_PARSE_EQ(float, "1", 1.0f);
    EXPECT_CONV_PARSE_EQ(float, "1.0", 1.0f);
    EXPECT_CONV_PARSE_EQ(float, "-1", -1.0f);
    EXPECT_CONV_PARSE_EQ(float, "-1.0", -1.0f);
    EXPECT_CONV_PARSE_EQ(float, "0.00001", 0.00001f);
    EXPECT_CONV_PARSE_EQ(float, "-0.00001", -0.00001f);
    EXPECT_CONV_PARSE_EQ(float, "9999999.99991", 9999999.99991f);
    EXPECT_CONV_PARSE_EQ(float, "-9999999.99991", -9999999.99991f);

    EXPECT_CONV_PARSE_EQ(double, "0", 0.0);
    EXPECT_CONV_PARSE_EQ(double, "0.0", 0.0);
    EXPECT_CONV_PARSE_EQ(double, "1", 1.0);
    EXPECT_CONV_PARSE_EQ(double, "1.0", 1.0);
    EXPECT_CONV_PARSE_EQ(double, "-1", -1.0);
    EXPECT_CONV_PARSE_EQ(double, "-1.0", -1.0);
    EXPECT_CONV_PARSE_EQ(double, "0.00001", 0.00001);
    EXPECT_CONV_PARSE_EQ(double, "-0.00001", -0.00001);
    EXPECT_CONV_PARSE_EQ(double, "9999999.99991", 9999999.99991);
    EXPECT_CONV_PARSE_EQ(double, "-9999999.99991", -9999999.99991);
}


TEST(jxc_cpp_converter, ConverterSerializeFloats)
{
    EXPECT_CONV_SERIALIZE_EQ(-99999.5f, "-99999.5");
    EXPECT_CONV_SERIALIZE_EQ(-1.0f, "-1.0");
    EXPECT_CONV_SERIALIZE_EQ(0.0f, "0.0");
    EXPECT_CONV_SERIALIZE_EQ(1.0f, "1.0");
    EXPECT_CONV_SERIALIZE_EQ(99999.5f, "99999.5");

    EXPECT_CONV_SERIALIZE_EQ(-99999.5, "-99999.5");
    EXPECT_CONV_SERIALIZE_EQ(-1.0, "-1.0");
    EXPECT_CONV_SERIALIZE_EQ(0.0, "0.0");
    EXPECT_CONV_SERIALIZE_EQ(1.0, "1.0");
    EXPECT_CONV_SERIALIZE_EQ(99999.5, "99999.5");
}


TEST(jxc_cpp_converter, ConverterParseChars)
{
    EXPECT_CONV_PARSE_EQ(char, "74", 'J');
    EXPECT_CONV_PARSE_EQ(char, "'J'", 'J');

    // FIXME: this worked at one point...
// #if JXC_CPP20
//     EXPECT_CONV_PARSE_EQ(char8_t, "74", u8'J');
//     EXPECT_CONV_PARSE_EQ(char8_t, "'J'", u8'J');
// #endif

    EXPECT_CONV_PARSE_EQ(wchar_t, "74", L'J');
    EXPECT_CONV_PARSE_EQ(wchar_t, "'J'", L'J');

    EXPECT_CONV_PARSE_EQ(char16_t, "74", u'J');
    EXPECT_CONV_PARSE_EQ(char16_t, "'J'", u'J');
    EXPECT_CONV_PARSE_EQ(char16_t, "0x2192", u'→');
    EXPECT_CONV_PARSE_EQ(char16_t, R"('\u2192')", u'→');
    EXPECT_CONV_PARSE_EQ(char16_t, "'→'", u'→');
    EXPECT_CONV_PARSE_NE(char16_t, "'←'", u'→');
    EXPECT_THROW(jxc::conv::parse<char16_t>("'→←'"), jxc::parse_error);

    EXPECT_CONV_PARSE_EQ(char32_t, "74", U'J');
    EXPECT_CONV_PARSE_EQ(char32_t, "'J'", U'J');
    EXPECT_CONV_PARSE_EQ(char32_t, "0x1F62E", U'😮');
    EXPECT_CONV_PARSE_EQ(char32_t, "'😮'", U'😮');
    EXPECT_CONV_PARSE_EQ(char32_t, R"('\U0001F62E')", U'😮');
}


TEST(jxc_cpp_converter, ConverterSerializeChars)
{
    EXPECT_CONV_SERIALIZE_EQ('A', "\"A\"");
#if JXC_CPP20
    EXPECT_CONV_SERIALIZE_EQ(u8'A', "\"A\"");
#endif
    EXPECT_CONV_SERIALIZE_EQ(L'A', "\"A\"");
    EXPECT_CONV_SERIALIZE_EQ(u'A', "\"A\"");
    EXPECT_CONV_SERIALIZE_EQ(U'A', "\"A\"");
}


TEST(jxc_cpp_converter, ConverterParseStrings)
{
    EXPECT_CONV_PARSE_EQ(std::string, "''", "");
    EXPECT_CONV_PARSE_EQ(std::string, "'abc'", "abc");
    EXPECT_CONV_PARSE_EQ(std::string, "'\\tabc\\t'", "\tabc\t");

    // raw strings pass through any escapes
    EXPECT_CONV_PARSE_EQ(std::string, "r'()'", "");
    EXPECT_CONV_PARSE_EQ(std::string, R"( r'(\tabc\t)' )", R"(\tabc\t)");

    // convert string to vector<char>
    {
        auto result = jxc::conv::parse<std::vector<char>>(R"( 'abc' )");
        EXPECT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], 'a');
        EXPECT_EQ(result[1], 'b');
        EXPECT_EQ(result[2], 'c');
    }

    // == Escape Types ==

    // ascii escapes
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\0')", 0x00);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\a')", 0x07);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\b')", 0x08);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\t')", 0x09);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\n')", 0x0a);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\v')", 0x0b);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\f')", 0x0c);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\r')", 0x0d);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\"')", 0x22);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\'')", 0x27);
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<char>, R"('\\')", 0x5c);

    // one-byte hex escapes
    EXPECT_CONV_PARSE_EQ(std::string, R"( '\x41' )", "A");
    EXPECT_CONV_PARSE_EQ(std::string, R"( '\x41\x42\x43' )", "ABC");

    // two-byte utf16 escapes
    EXPECT_CONV_PARSE_EQ(std::string, R"( '\u2192' )", "→");
    EXPECT_CONV_PARSE_EQ(std::string, R"( ' \u2192 \u2190 ' )", " → ← ");

    // four-byte utf32 escapes
    EXPECT_CONV_PARSE_EQ(std::string, R"( '\U0001F62E' )", "😮");
    EXPECT_CONV_PARSE_EQ(std::string, R"( ' \U0001F62E \U0001F4BD ' )", " 😮 💽 ");
}


TEST(jxc_cpp_converter, ConverterSerializeStrings)
{
    EXPECT_CONV_SERIALIZE_EQ(std::string(""), "\"\"");
    EXPECT_CONV_SERIALIZE_EQ(std::string("abc"), "\"abc\"");
    EXPECT_CONV_SERIALIZE_EQ(std::string("\tabc\t"), R"("\tabc\t")");
}


TEST(jxc_cpp_converter, ConverterParsePaths)
{
    EXPECT_CONV_PARSE_EQ(std::filesystem::path, "'/a/b/c'", fs::path("/a/b/c"));
}


TEST(jxc_cpp_converter, ConverterSerializePaths)
{
    EXPECT_CONV_SERIALIZE_EQ(fs::path("/a/b/c"), "r\"(/a/b/c)\"");
    EXPECT_CONV_SERIALIZE_EQ(fs::path("\\a\\b\\c"), "r\"(\\a\\b\\c)\"");
}


TEST(jxc_cpp_converter, ConverterParseBytes)
{
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<uint8_t>, R"( b64'' )");
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<uint8_t>, R"( b64'anhj' )", 'j', 'x', 'c');
    EXPECT_CONV_PARSE_BYTES_EQ(std::vector<uint8_t>, R"( b64'( S l h D A A A A )' )", 'J', 'X', 'C', 0, 0, 0);
}


TEST(jxc_cpp_converter, ConverterSerializeBytes)
{
    EXPECT_CONV_SERIALIZE_EQ(std::vector<uint8_t>(), "b64\"\"");
    EXPECT_CONV_SERIALIZE_EQ((std::vector<uint8_t>{ 'j', 'x', 'c' }), "b64\"anhj\"");
}


TEST(jxc_cpp_converter, ConverterParseDateTimes)
{
    EXPECT_CONV_PARSE_EQ(jxc::Date, "dt'2007-04-30'", jxc::Date(2007, 4, 30));
    EXPECT_CONV_PARSE_EQ(jxc::DateTime, "dt'2007-04-30T17:32:01Z'", jxc::DateTime(2007, 4, 30, 17, 32, 1, 0));
}


TEST(jxc_cpp_converter, ConverterSerializeDateTimes)
{
    EXPECT_CONV_SERIALIZE_EQ(jxc::Date(2007, 4, 30), "dt\"2007-04-30\"");
    EXPECT_CONV_SERIALIZE_EQ(jxc::DateTime(2007, 4, 30, 17, 32, 1, 0), "dt\"2007-04-30T17:32:01Z\"");
}


TEST(jxc_cpp_converter, ConverterParseArrays)
{
    {
        auto result = jxc::conv::parse<std::vector<int32_t>>(
            "[ -2, 12, 42 ]");
        EXPECT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], -2);
        EXPECT_EQ(result[1], 12);
        EXPECT_EQ(result[2], 42);
    }

    {
        auto result = jxc::conv::parse<std::vector<std::string>>(
            "[ '', 'jxc' ]");
        EXPECT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], "");
        EXPECT_EQ(result[1], "jxc");
    }

    {
        auto result = jxc::conv::parse<std::vector<int32_t>>("std.vector<int32_t>[-9999,0,2342343]");
        EXPECT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], -9999);
        EXPECT_EQ(result[1], 0);
        EXPECT_EQ(result[2], 2342343);
    }

    {
        auto result = jxc::conv::parse<std::vector<std::vector<int8_t>>>(
            "[ [-128, 0, 127], [], [5] ]");
        EXPECT_EQ(result.size(), 3);

        EXPECT_EQ(result[0].size(), 3);
        EXPECT_EQ(result[0][0], -128);
        EXPECT_EQ(result[0][1], 0);
        EXPECT_EQ(result[0][2], 127);

        EXPECT_EQ(result[1].size(), 0);

        EXPECT_EQ(result[2].size(), 1);
        EXPECT_EQ(result[2][0], 5);
    }

    // std::array
    {
        std::array<int16_t, 3> result = jxc::conv::parse<std::array<int16_t, 3>>("[-5, 75, 32000]");
        EXPECT_EQ(result.size(), 3);
        EXPECT_EQ(result[0], -5);
        EXPECT_EQ(result[1], 75);
        EXPECT_EQ(result[2], 32000);
    }

    {
        auto result = jxc::conv::parse<std::vector<std::array<int16_t, 3>>>("[[0, 0, 0], [-5, 75, 32000]]");
        EXPECT_EQ(result.size(), 2);

        EXPECT_EQ(result[0].size(), 3);
        EXPECT_EQ(result[0][0], 0);
        EXPECT_EQ(result[0][1], 0);
        EXPECT_EQ(result[0][2], 0);

        EXPECT_EQ(result[1].size(), 3);
        EXPECT_EQ(result[1][0], -5);
        EXPECT_EQ(result[1][1], 75);
        EXPECT_EQ(result[1][2], 32000);
    }

    // std::set
    {
        std::set<int32_t> result = jxc::conv::parse<std::set<int32_t>>("[ 0, 1, -523, 1, -523 ]");
        EXPECT_EQ(result.size(), 3);
        EXPECT_TRUE(result.contains(0));
        EXPECT_TRUE(result.contains(1));
        EXPECT_TRUE(result.contains(-523));
    }

    // std::unordered_set
    {
        std::unordered_set<int32_t> result = jxc::conv::parse<std::unordered_set<int32_t>>("[ 0, 1, -523, 1, -523 ]");
        EXPECT_EQ(result.size(), 3);
        EXPECT_TRUE(result.contains(0));
        EXPECT_TRUE(result.contains(1));
        EXPECT_TRUE(result.contains(-523));
    }
}


TEST(jxc_cpp_converter, ConverterSerializeArrays)
{
    EXPECT_CONV_SERIALIZE_EQ((std::vector<int32_t>({ -9999, 0, 2342343 })), "std.vector<int32_t>[-9999,0,2342343]");
}


TEST(jxc_cpp_converter, ConverterParseTuples)
{
    {
        auto result = jxc::conv::parse<std::tuple<int, int, bool>>("[0, 1, false]");
        EXPECT_EQ(std::get<0>(result), 0);
        EXPECT_EQ(std::get<1>(result), 1);
        EXPECT_EQ(std::get<2>(result), false);
        EXPECT_NE(std::get<2>(result), true);
    }

    {
        auto result = jxc::conv::parse<std::tuple<int32_t, bool, std::optional<std::string>, std::string>>(
            "[ 1, true, null, 'abc' ]");
        EXPECT_EQ(std::get<0>(result), 1);
        EXPECT_EQ(std::get<1>(result), true);
        EXPECT_EQ(std::get<2>(result), std::nullopt);
        EXPECT_EQ(std::get<3>(result), "abc");
        EXPECT_NE(std::get<3>(result), "abc!!!!!");
    }
}


TEST(jxc_cpp_converter, ConverterSerializeTuples)
{
    EXPECT_CONV_SERIALIZE_EQ(std::make_tuple(5), "[5]");
    EXPECT_CONV_SERIALIZE_EQ(std::make_tuple(nullptr, true, 123), "[null,true,123]");
    EXPECT_CONV_SERIALIZE_EQ(std::make_tuple(12345, std::string("abc"), true, -5.1234), "[12345,\"abc\",true,-5.1234]");
    EXPECT_CONV_SERIALIZE_EQ(std::make_tuple(-7, false, std::make_tuple(1.0, 2.0, 3.0)), "[-7,false,[1.0,2.0,3.0]]");
}


TEST(jxc_cpp_converter, ConverterParseObjects)
{
    // std::map
    {
        auto result = jxc::conv::parse<std::map<std::string, int64_t>>("{ a: 5, b: -99999999, '!!': 99999999999 }");
        EXPECT_EQ(result.size(), 3);

        EXPECT_TRUE(result.contains("a"));
        EXPECT_EQ(result["a"], 5);

        EXPECT_TRUE(result.contains("b"));
        EXPECT_EQ(result["b"], -99999999);

        EXPECT_TRUE(result.contains("!!"));
        EXPECT_EQ(result["!!"], 99999999999);
    }

    // std::unordered_map
    {
        auto result = jxc::conv::parse<std::unordered_map<int32_t, std::string>>(
            "{ 5: 'abc', 42: 'jxc' }");
        EXPECT_EQ(result.size(), 2);
        EXPECT_TRUE(result.contains(5));
        EXPECT_EQ(result[5], "abc");
        EXPECT_TRUE(result.contains(42));
        EXPECT_EQ(result[42], "jxc");
    }

    {
        auto result = jxc::conv::parse<std::unordered_map<std::string, std::vector<int32_t>>>(
            "{a:[], b:[1,-5], 'c-!':[99999,0,-1]}");
        EXPECT_EQ(result.size(), 3);

        EXPECT_TRUE(result.contains("a"));
        EXPECT_EQ(result["a"].size(), 0);

        EXPECT_TRUE(result.contains("b"));
        EXPECT_EQ(result["b"].size(), 2);
        EXPECT_EQ(result["b"][0], 1);
        EXPECT_EQ(result["b"][1], -5);

        EXPECT_TRUE(result.contains("c-!"));
        EXPECT_EQ(result["c-!"].size(), 3);
        EXPECT_EQ(result["c-!"][0], 99999);
        EXPECT_EQ(result["c-!"][1], 0);
        EXPECT_EQ(result["c-!"][2], -1);
    }
}


// helper for creating an unordered_map with exactly one entry
template<typename KT, typename VT>
std::unordered_map<KT, VT> make_unordered_map(const KT& key, const VT& val)
{
    return std::unordered_map<KT, VT>{ { key, val } };
}


TEST(jxc_cpp_converter, ConverterSerializeObjects)
{
    EXPECT_CONV_SERIALIZE_EQ(make_unordered_map(0, 5),
        "std.unordered_map<int32_t, int32_t>{0:5}");
    EXPECT_CONV_SERIALIZE_EQ(make_unordered_map(std::string("abc"), std::string("def")),
        "std.unordered_map<string, string>{abc:\"def\"}");
}


TEST(jxc_cpp_converter, ConverterParseOptional)
{
    {
        auto result = jxc::conv::parse<std::optional<bool>>("null");
        EXPECT_FALSE(result.has_value());
    }

    {
        auto result = jxc::conv::parse<std::optional<bool>>("true");
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result, true);
        EXPECT_EQ(result.value_or(false), true);
    }

    {
        auto result = jxc::conv::parse<std::vector<std::optional<int32_t>>>("[ null, 0, -5431, null, 999999 ]");
        EXPECT_EQ(result.size(), 5);
        EXPECT_FALSE(result[0].has_value());
        EXPECT_EQ(result[1], 0);
        EXPECT_EQ(result[2], -5431);
        EXPECT_FALSE(result[3].has_value());
        EXPECT_EQ(result[4], 999999);
    }
}


TEST(jxc_cpp_converter, ConverterSerializeOptional)
{
    EXPECT_CONV_SERIALIZE_EQ(std::optional<double>(), "null");
    EXPECT_CONV_SERIALIZE_EQ(std::optional<int32_t>(1234), "1234");
    EXPECT_CONV_SERIALIZE_EQ((std::vector<std::optional<int32_t>>{ 0, -7, std::nullopt, 12345 }), "std.vector<int32_t>[0,-7,null,12345]");
    EXPECT_CONV_SERIALIZE_EQ((std::vector<std::optional<std::vector<int32_t>>>{ std::nullopt }), "std.vector<std.vector<int32_t>>[null]");
}


TEST(jxc_cpp_converter, ConverterParsePointers)
{
    {
        std::unique_ptr<int32_t> result = jxc::conv::parse<std::unique_ptr<int32_t>>("12345");
        EXPECT_NE(result.get(), nullptr);
        EXPECT_EQ(*result.get(), 12345);
    }

    {
        auto result = jxc::conv::parse<std::shared_ptr<std::vector<std::string>>>("null");
        EXPECT_EQ(result, nullptr);
    }

    {
        auto result = jxc::conv::parse<std::shared_ptr<std::vector<std::string>>>("[ '', 'abc' ]");
        EXPECT_NE(result, nullptr);
        EXPECT_EQ(result->at(0), "");
        EXPECT_EQ(result->at(1), "abc");
    }
}


TEST(jxc_cpp_converter, ConverterSerializePointers)
{
    EXPECT_CONV_SERIALIZE_EQ(std::make_unique<int32_t>(5), "5");
    EXPECT_CONV_SERIALIZE_EQ(std::unique_ptr<int32_t>(), "null");

    EXPECT_CONV_SERIALIZE_EQ(std::make_shared<int32_t>(5), "5");
    EXPECT_CONV_SERIALIZE_EQ(std::shared_ptr<int32_t>(), "null");
}


enum class TestEnumA : uint8_t
{
    None = 0,
    First,
    Second,
    Third,
    Fourth = 77,
    Fifth = 200,
    Last,
};


JXC_DEFINE_AUTO_ENUM_CONVERTER(
    TestEnumA,
    "TestEnumA",
    jxc::EnumConverterStyle::ValueAsInteger,
    JXC_ENUM_VALUE(None),
    JXC_ENUM_VALUE(First),
    JXC_ENUM_VALUE(Second),
    JXC_ENUM_VALUE(Third),
    JXC_ENUM_VALUE(Fourth),
    JXC_ENUM_VALUE(Fifth),
    JXC_ENUM_VALUE(Last),
);


enum class TestEnumB : int16_t
{
    None = 0,
    First,
    Second,
    Third,
    Fourth = 42,
    Last,
    Extra = -5,
};


JXC_DEFINE_AUTO_ENUM_CONVERTER(
    TestEnumB,
    "TestEnumB",
    jxc::EnumConverterStyle::NameAsExpression,
    JXC_ENUM_VALUE(None),
    JXC_ENUM_VALUE(First),
    JXC_ENUM_VALUE(Second),
    JXC_ENUM_VALUE(Third),
    JXC_ENUM_VALUE(Fourth),
    JXC_ENUM_VALUE(Last),
    JXC_ENUM_VALUE(Extra),
);


enum class TestEnumC : int32_t
{
    None = 0,
    First,
    Second,
    Third,
    Fourth = 1005,
    Fifth = 64000,
    Last,
};


JXC_DEFINE_AUTO_ENUM_CONVERTER(
    TestEnumC,
    "TestEnumC",
    jxc::EnumConverterStyle::NameAsString,
    JXC_ENUM_VALUE(None),
    JXC_ENUM_VALUE(First),
    JXC_ENUM_VALUE(Second),
    JXC_ENUM_VALUE(Third),
    JXC_ENUM_VALUE(Fourth),
    JXC_ENUM_VALUE(Fifth),
    JXC_ENUM_VALUE(Last),
);


TEST(jxc_cpp_converter, ConverterAutoEnum)
{
    // TestEnumA
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA 0", TestEnumA::None);
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA 1", TestEnumA::First);
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA(Second)", TestEnumA::Second);
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA 'Third'", TestEnumA::Third);
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA(77)", TestEnumA::Fourth);
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA(Fifth)", TestEnumA::Fifth);
    EXPECT_CONV_PARSE_EQ(TestEnumA, "TestEnumA(Last)", TestEnumA::Last);

    // TestEnumB
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB 0", TestEnumB::None);
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB 1", TestEnumB::First);
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB(Second)", TestEnumB::Second);
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB 'Third'", TestEnumB::Third);
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB(42)", TestEnumB::Fourth);
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB(Last)", TestEnumB::Last);
    EXPECT_CONV_PARSE_EQ(TestEnumB, "TestEnumB -5", TestEnumB::Extra);

    // TestEnumC
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC 0", TestEnumC::None);
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC 1", TestEnumC::First);
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC(Second)", TestEnumC::Second);
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC 'Third'", TestEnumC::Third);
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC(1005)", TestEnumC::Fourth);
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC(64000)", TestEnumC::Fifth);
    EXPECT_CONV_PARSE_EQ(TestEnumC, "TestEnumC 'Last'", TestEnumC::Last);

    // invalid values
    EXPECT_THROW(jxc::conv::parse<TestEnumC>("TestEnumC 999"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<TestEnumC>("TestEnumC(InvalidValue)"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<TestEnumC>("TestEnumC(a + b)"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<TestEnumC>("TestEnumC('!!')"), jxc::parse_error);
    EXPECT_THROW(jxc::conv::parse<TestEnumC>("TestEnumC ''"), jxc::parse_error);

    // ValueAsInteger style
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::None, "TestEnumA 0");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::First, "TestEnumA 1");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::Second, "TestEnumA 2");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::Third, "TestEnumA 3");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::Fourth, "TestEnumA 77");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::Fifth, "TestEnumA 200");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumA::Last, "TestEnumA 201");

    // NameAsExpression style
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::None, "TestEnumB(None)");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::First, "TestEnumB(First)");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::Second, "TestEnumB(Second)");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::Third, "TestEnumB(Third)");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::Fourth, "TestEnumB(Fourth)");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::Last, "TestEnumB(Last)");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumB::Extra, "TestEnumB(Extra)");

    // NameAsString style
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::None, "TestEnumC \"None\"");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::First, "TestEnumC \"First\"");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::Second, "TestEnumC \"Second\"");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::Third, "TestEnumC \"Third\"");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::Fourth, "TestEnumC \"Fourth\"");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::Fifth, "TestEnumC \"Fifth\"");
    EXPECT_CONV_SERIALIZE_EQ(TestEnumC::Last, "TestEnumC \"Last\"");
}


struct TestSimpleAutoStruct
{
    int64_t id = 0;
    std::string name;
    std::optional<std::string> url;
    double alpha = 1.0;

    // equality operator required for tests to work
    inline bool operator==(const TestSimpleAutoStruct& rhs) const
    {
        return id == rhs.id && name == rhs.name && url == rhs.url && alpha == rhs.alpha;
    }
};


JXC_DEFINE_AUTO_STRUCT_CONVERTER(
    TestSimpleAutoStruct,
    "TestSimpleAutoStruct",
    JXC_FIELD(id),
    JXC_FIELD(name),
    JXC_FIELD(url),
    JXC_FIELD(alpha, jxc::FieldFlags::Optional,
        [](jxc::Serializer& doc, const TestSimpleAutoStruct& value) {
            doc.value_float(value.alpha, "", 4, true);
        }),
);


TEST(jxc_cpp_converter, TestSimpleAutoStructTests)
{
    EXPECT_CONV_PARSE_EQ(
        TestSimpleAutoStruct,
        "TestSimpleAutoStruct{ id: 999123, name: 'jxc', url: null }",
        (TestSimpleAutoStruct{ 999123, "jxc", std::nullopt, 1.0 }));

    EXPECT_CONV_SERIALIZE_EQ(
        (TestSimpleAutoStruct{ 999123, "jxc", std::nullopt, 1.5 }),
        "TestSimpleAutoStruct{id:999123,name:\"jxc\",url:null,alpha:1.5000}");
}


struct TestCustomizedAutoStruct
{
    std::string name;
    int32_t value = 0;
    double weight = 0.0;

    // equality operator required for tests to work
    inline bool operator==(const TestCustomizedAutoStruct& rhs) const
    {
        return name == rhs.name && value == rhs.value && weight == rhs.weight;
    }
};


JXC_DEFINE_AUTO_STRUCT_CONVERTER(
    TestCustomizedAutoStruct,
    "TestCustomizedAutoStruct",
    jxc::def_field("name", &TestCustomizedAutoStruct::name,
        [](jxc::Serializer& doc, const TestCustomizedAutoStruct& value)
        {
            doc.value_string_raw(value.name);
        },
        [](jxc::conv::Parser& parser, TestCustomizedAutoStruct& out_value)
        {
            out_value.name = parser.parse_value<decltype(out_value.name)>();
        }),
    jxc::def_field("value", &TestCustomizedAutoStruct::value,
        [](jxc::Serializer& doc, const TestCustomizedAutoStruct& value)
        {
            doc.value_int_hex(value.value);
        },
        [](jxc::conv::Parser& parser, const std::string& field_key, TestCustomizedAutoStruct& out_value)
        {
            out_value.value = parser.parse_value<decltype(out_value.value)>();
            if (out_value.value < -100 || out_value.value > 100)
            {
                throw jxc::parse_error(jxc::format("{} {} out of bounds (valid range is -100 to 100)", field_key, out_value.value), parser.value());
            }
        }),
    jxc::def_field("weight", &TestCustomizedAutoStruct::weight,
        [](jxc::Serializer& doc, const TestCustomizedAutoStruct& value)
        {
            doc.value_float(value.weight, std::string_view(), 8, true);
        },
        [](jxc::conv::Parser& parser, TestCustomizedAutoStruct& out_value)
        {
            out_value.weight = parser.parse_value<decltype(out_value.weight)>();
        })
);


TEST(jxc_cpp_converter, TestCustomizedAutoStructTests)
{
    EXPECT_CONV_PARSE_EQ(
        TestCustomizedAutoStruct,
        "TestCustomizedAutoStruct{ name: 'abc123', value: -5, weight: 2.0 }",
        (TestCustomizedAutoStruct{ "abc123", -5, 2.0 }));
    
    // value out of range
    EXPECT_THROW(jxc::conv::parse<TestCustomizedAutoStruct>(
        "TestCustomizedAutoStruct{ name: '', value: -999, weight: 2.0 }"),
        jxc::parse_error);

    EXPECT_CONV_SERIALIZE_EQ(
        (TestCustomizedAutoStruct{ "abc123", -5, 2.0 }),
        "TestCustomizedAutoStruct{name:r\"(abc123)\",value:-0x5,weight:2.00000000}");
}


struct TestFullyCustomStruct
{
    bool flag = false;
    std::vector<int32_t> numbers;
    jxc::Date created;

    // equality operator required for tests to work
    inline bool operator==(const TestFullyCustomStruct& rhs) const
    {
        return flag == rhs.flag && numbers == rhs.numbers && created == rhs.created;
    }
};


template<>
struct jxc::Converter<TestFullyCustomStruct>
{
    using value_type = TestFullyCustomStruct;

    static std::string get_annotation()
    {
        return "TestFullyCustomStruct";
    }

    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        doc.annotation(get_annotation());
        doc.object_begin();
        doc.identifier("flag").sep().value_bool(value.flag);
        doc.identifier("numbers").sep();
        doc.array_begin();
        for (const auto& v : value.numbers)
        {
            doc.value_int(v);
        }
        doc.array_end();
        doc.identifier("created").sep().value_date(value.created);
        doc.object_end();
    }

    static value_type parse(jxc::conv::Parser& parser, jxc::TokenSpan generic_anno)
    {
        value_type result;

        bool parsed_flag = false;
        bool parsed_numbers = false;
        bool parsed_created = false;

        parser.require(jxc::ElementType::BeginObject);

        if (TokenSpan struct_anno = parser.get_value_annotation(generic_anno))
        {
            auto anno_parser = parser.parse_annotation(struct_anno);
            anno_parser.require_then_advance(jxc::TokenType::Identifier, get_annotation());
            anno_parser.done_required();
        }

        while (parser.next())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }

            const std::string key = parser.parse_token_as_object_key<std::string>(parser.value().token);
            if (!parser.next())
            {
                throw jxc::parse_error("Unexpected end of stream");
            }

            if (key == "flag")
            {
                parsed_flag = true;
                result.flag = parser.parse_value<bool>();
            }
            else if (key == "numbers")
            {
                parsed_numbers = true;
                result.numbers = parser.parse_value<std::vector<int32_t>>();
            }
            else if (key == "created")
            {
                parsed_created = true;
                result.created = parser.parse_value<jxc::Date>();
            }
            else
            {
                throw jxc::parse_error(jxc::format("Invalid key {} for struct {}", key, get_annotation()), parser.value());
            }
        }

        if (!parsed_flag) { throw jxc::parse_error("Missing field 'flag'", parser.value()); }
        if (!parsed_numbers) { throw jxc::parse_error("Missing field 'numbers'", parser.value()); }
        if (!parsed_created) { throw jxc::parse_error("Missing field 'created'", parser.value()); }

        return result;
    }
};


TEST(jxc_cpp_converter, TestFullyCustomStructTests)
{
    EXPECT_CONV_PARSE_EQ(
        TestFullyCustomStruct,
        "TestFullyCustomStruct{ flag: false, numbers: [0, 1, -999, 2], created: dt'1997-01-31' }",
        (TestFullyCustomStruct{ false, { 0, 1, -999, 2 }, jxc::Date(1997, 1, 31) }));

    EXPECT_CONV_SERIALIZE_EQ(
        (TestFullyCustomStruct{ false, { 0, 1, -999, 2 }, jxc::Date(1997, 1, 31) }),
        "TestFullyCustomStruct{flag:false,numbers:[0,1,-999,2],created:dt\"1997-01-31\"}");

    // invalid field
    EXPECT_THROW(jxc::conv::parse<TestFullyCustomStruct>(
        "TestFullyCustomStruct{ flag: true, numbers: [], created: dt'2004-01-01', abc: true }"),
        jxc::parse_error);

    // missing field
    EXPECT_THROW(jxc::conv::parse<TestFullyCustomStruct>(
        "TestFullyCustomStruct{ flag: true, numbers: [] }"),
        jxc::parse_error);
}


//TODO: Add tests for structs with extra_field defined (JXC_EXTRA)


