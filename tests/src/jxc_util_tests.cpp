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


template<typename T>
struct StackVectorAllocator
{
    struct Tracker
    {
        std::unordered_map<T*, size_t> map;
        size_t num_allocs = 0;
        size_t num_frees = 0;
    };

    static Tracker& get_tracker()
    {
        static Tracker tracker;
        return tracker;
    }

    static size_t num_allocations()
    {
        return get_tracker().map.size();
    }

    static inline jxc::detail::FatPointer<T> alloc(size_t num_elements)
    {
        jxc::detail::FatPointer<T> result = { static_cast<T*>(::malloc(sizeof(T) * num_elements)), num_elements };
        result.memzero();
        get_tracker().num_allocs++;
        JXC_ASSERT(!get_tracker().map.contains(result.ptr));
        get_tracker().map.insert_or_assign(result.ptr, num_elements);
        return result;
    }

    static inline void free(jxc::detail::FatPointer<T> data)
    {
        JXC_ASSERT(get_tracker().map.contains(data.ptr));
        get_tracker().num_frees++;
        get_tracker().map.erase(data.ptr);
        ::free(data.ptr);
    }
};


template<typename T, uint16_t BufSize = 16>
using DebugStackVector = jxc::detail::StackVector<T, BufSize, StackVectorAllocator<T>>;


TEST(jxc_core, StackVector)
{
    {
        static constexpr int64_t large_arr_size = 100000;
        EXPECT_EQ(StackVectorAllocator<int64_t>::num_allocations(), 0);
        DebugStackVector<int64_t> vec;
        for (int64_t i = 0; i < large_arr_size; i++)
        {
            vec.push_back(i);
        }
        EXPECT_EQ(StackVectorAllocator<int64_t>::num_allocations(), 1);
        EXPECT_EQ(vec.size(), large_arr_size);
        for (int64_t i = 0; i < large_arr_size; i++)
        {
            EXPECT_EQ(vec[i], i);
        }

        DebugStackVector<int64_t> vec_moved = std::move(vec);
        EXPECT_EQ(vec_moved.size(), large_arr_size);
        for (int64_t i = 0; i < large_arr_size; i++)
        {
            EXPECT_EQ(vec_moved[i], i);
        }
    }

    EXPECT_EQ(StackVectorAllocator<int64_t>::get_tracker().num_allocs, StackVectorAllocator<int64_t>::get_tracker().num_frees);

    {
        DebugStackVector<int32_t> vec;
        EXPECT_EQ(StackVectorAllocator<int32_t>::num_allocations(), 0);
        vec.push_back(1234);
        EXPECT_EQ(StackVectorAllocator<int32_t>::num_allocations(), 0);
        vec.push_back(1234);
        EXPECT_EQ(StackVectorAllocator<int32_t>::num_allocations(), 0);
        vec.resize(32, 1);
        EXPECT_EQ(StackVectorAllocator<int32_t>::num_allocations(), 1);
        vec.clear(true);
        EXPECT_EQ(StackVectorAllocator<int32_t>::num_allocations(), 0);
    }

    EXPECT_EQ(StackVectorAllocator<int32_t>::num_allocations(), 0);

    // store weak pointers to all values so we can verify they're all invalid later
    std::vector<std::weak_ptr<int32_t>> allocated_values;

    {
        DebugStackVector<std::shared_ptr<int32_t>> vec;
        for (int32_t i = 0; i < 32; i++)
        {
            auto value = std::make_shared<int32_t>(i);
            vec.push_back(value);
            allocated_values.push_back(value);
        }
        EXPECT_EQ(StackVectorAllocator<std::shared_ptr<int32_t>>::num_allocations(), 1);
        DebugStackVector<std::shared_ptr<int32_t>> vec_copy = vec;
        EXPECT_EQ(StackVectorAllocator<std::shared_ptr<int32_t>>::num_allocations(), 2);
        EXPECT_EQ(vec[30].get(), vec_copy[30].get()); // compare by pointer
        EXPECT_EQ(*vec[30], *vec_copy[30]); // compare by value
        *vec[30] = -10;
        EXPECT_EQ(*vec_copy[30], -10);

        size_t idx = 0;
        for (const auto& value : vec)
        {
            EXPECT_EQ(allocated_values.at(idx).lock().get(), value.get()); // compare by pointer
            ++idx;
        }
    }

    EXPECT_EQ(StackVectorAllocator<std::shared_ptr<int32_t>>::num_allocations(), 0);

    // all allocated values should have been destroyed
    for (const std::weak_ptr<int32_t>& value_weak : allocated_values)
    {
        std::shared_ptr<int32_t> value_shared = value_weak.lock();
        EXPECT_EQ(value_shared.get(), nullptr);
    }
}


TEST(jxc_core, TokenLists)
{
    using namespace jxc;

    std::string anno_source = "array<set<string>>";

    auto make_token_view = [&anno_source](TokenType type, size_t start_idx, size_t length) -> Token
    {
        JXC_ASSERT(length > 0);
        JXC_ASSERT(start_idx < anno_source.size());
        JXC_ASSERT(start_idx + length <= anno_source.size());
        return Token(
            type,
            start_idx,
            start_idx + length,
            FlexString::make_view(std::string_view(anno_source).substr(start_idx, length)));
    };

    // instead of parsing the annotation, we'll specify the substring slices and token types manually
    std::vector<Token> tok_data = {
        make_token_view(TokenType::Identifier, 0, 5),
        make_token_view(TokenType::AngleBracketOpen, 5, 1),
            make_token_view(TokenType::Identifier, 6, 3),
            make_token_view(TokenType::AngleBracketOpen, 9, 1),
                make_token_view(TokenType::Identifier, 10, 6),
            make_token_view(TokenType::AngleBracketClose, 16, 1),
        make_token_view(TokenType::AngleBracketClose, 17, 1)
    };

    // make a TokenView view based on the owned vector
    TokenView span{ tok_data[0], tok_data.size(), anno_source };

    // make sure our manual string index numbers are correct
    EXPECT_EQ(tok_data[0].value, "array");
    EXPECT_EQ(tok_data[1].value, "<");
    EXPECT_EQ(tok_data[2].value, "set");
    EXPECT_EQ(tok_data[3].value, "<");
    EXPECT_EQ(tok_data[4].value, "string");
    EXPECT_EQ(tok_data[5].value, ">");
    EXPECT_EQ(tok_data[6].value, ">");

    // make sure the TokenView tokens match the ones in the owned tok_data vector
    EXPECT_EQ(span.size(), tok_data.size());
    EXPECT_EQ(span[0], tok_data[0]);
    EXPECT_EQ(span[1], tok_data[1]);
    EXPECT_EQ(span[2], tok_data[2]);
    EXPECT_EQ(span[3], tok_data[3]);
    EXPECT_EQ(span[4], tok_data[4]);
    EXPECT_EQ(span[5], tok_data[5]);
    EXPECT_EQ(span[6], tok_data[6]);

    // the span should equal itself (using a whitespace-independent equality function)
    EXPECT_TRUE(span.equals_annotation_string_lexed(" array < set < string > > "));
    EXPECT_FALSE(span.equals_annotation_string_lexed("std.array < std.set < string > > "));

    // the span's source should be identical to a copy of our original source
    EXPECT_EQ(span.source(), std::string(anno_source));

    // make sure we can use slice to get the inner generic parts
    TokenView inner = span.slice(2, 4);
    EXPECT_EQ(inner.size(), 4);
    EXPECT_EQ(inner.source(), std::string_view("set<string>"));
    EXPECT_TRUE(inner.equals_annotation_string_lexed(" set < string > "));
    EXPECT_FALSE(inner.equals_annotation_string_lexed(" set < std.string > "));

    // test slice params
    EXPECT_EQ(span.size(), 7);
    EXPECT_EQ(span.slice(4).size(), 3);
    EXPECT_EQ(span.slice(5).size(), 2);
    EXPECT_EQ(span.slice(6).size(), 1);
    EXPECT_EQ(span.slice(7).start, nullptr);
    EXPECT_EQ(span.slice(4, 999999).size(), 3);
    EXPECT_EQ(span.slice(5, 999999).size(), 2);
    EXPECT_EQ(span.slice(6, 999999).size(), 1);
    EXPECT_EQ(span.slice(7, 999999).start, nullptr);

    // test slice method on TokenList as well
    TokenList toklist = TokenList(span);

    // the span's source should be identical to a copy of our original source
    EXPECT_EQ(toklist.source(), std::string(anno_source));

    // make sure we can use slice to get the inner generic parts
    TokenList inner_copy = toklist.slice_copy(2, 4);
    EXPECT_EQ(inner_copy.size(), 4);
    EXPECT_EQ(inner_copy.source(), std::string_view("set<string>"));
    EXPECT_TRUE(TokenView(inner_copy).equals_annotation_string_lexed(" set < string > "));
    EXPECT_FALSE(TokenView(inner_copy).equals_annotation_string_lexed(" set < std.string > "));

    // test slice_copy params
    EXPECT_EQ(toklist.slice_copy(4).size(), 3);
    EXPECT_EQ(toklist.slice_copy(5).size(), 2);
    EXPECT_EQ(toklist.slice_copy(6).size(), 1);
    EXPECT_EQ(toklist.slice_copy(7).size(), 0);
    EXPECT_EQ(toklist.slice_copy(4, 999999).size(), 3);
    EXPECT_EQ(toklist.slice_copy(5, 999999).size(), 2);
    EXPECT_EQ(toklist.slice_copy(6, 999999).size(), 1);
    EXPECT_EQ(toklist.slice_copy(7, 999999).size(), 0);
}


TEST(jxc_util, AnnotationParserTest)
{
    using namespace jxc;

    std::string err;
    std::optional<TokenList> anno_opt = TokenList::parse_annotation("std.pair<int32_t(x.x | y, z), std.vector<double>>", &err);
    ASSERT_TRUE(anno_opt.has_value()) << err;

    AnnotationParser parser{ TokenView(*anno_opt) };
    
    EXPECT_EQ(parser.current().value, "std");
    EXPECT_TRUE(parser.advance());
    EXPECT_EQ(parser.current().type, TokenType::Period);
    EXPECT_TRUE(parser.advance());
    EXPECT_EQ(parser.current().value, "pair");
    EXPECT_TRUE(parser.advance());
    EXPECT_EQ(parser.current().type, TokenType::AngleBracketOpen);

    // read the first generic value
    {
        AnnotationParser generic_parser{ parser.skip_over_generic_value() };
        EXPECT_EQ(generic_parser.current().value, "int32_t");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::ParenOpen);
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().value, "x");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::Period);
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().value, "x");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::Pipe);
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().value, "y");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::Comma);
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().value, "z");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::ParenClose);
        EXPECT_FALSE(generic_parser.advance());
    }

    EXPECT_EQ(parser.current().type, TokenType::Comma);

    // read the second generic value after the comma
    {
        AnnotationParser generic_parser{ parser.skip_over_generic_value() };
        EXPECT_EQ(generic_parser.current().value, "std");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::Period);
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().value, "vector");
        EXPECT_TRUE(generic_parser.advance());
        EXPECT_EQ(generic_parser.current().type, TokenType::AngleBracketOpen);

        // read the inner generic value
        {
            TokenView generic_anno3 = generic_parser.skip_over_generic_value();
            EXPECT_EQ(generic_anno3.size(), 1);
            EXPECT_EQ(generic_anno3[0].value, "double");
        }

        EXPECT_EQ(generic_parser.current().type, TokenType::AngleBracketClose);
        EXPECT_FALSE(generic_parser.advance());
    }

    EXPECT_EQ(parser.current().type, TokenType::AngleBracketClose);
    EXPECT_FALSE(parser.advance());
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


TEST(jxc_util, IdentifierChecks)
{
    EXPECT_FALSE(jxc::is_valid_identifier(""));
    EXPECT_TRUE(jxc::is_valid_identifier("abc"));
    EXPECT_TRUE(jxc::is_valid_identifier("Abc"));
    EXPECT_FALSE(jxc::is_valid_identifier("Abc!"));
    EXPECT_FALSE(jxc::is_valid_identifier("Abc.Def"));

    EXPECT_FALSE(jxc::is_valid_dotted_identifier(""));
    EXPECT_TRUE(jxc::is_valid_dotted_identifier("Abc"));
    EXPECT_TRUE(jxc::is_valid_dotted_identifier("Abc.Def"));
    EXPECT_TRUE(jxc::is_valid_dotted_identifier("Abc.Def1"));
    EXPECT_FALSE(jxc::is_valid_dotted_identifier("Abc.Def1.1"));
    EXPECT_FALSE(jxc::is_valid_dotted_identifier("Abc.Def."));
    EXPECT_FALSE(jxc::is_valid_dotted_identifier(".Abc.Def"));
    EXPECT_FALSE(jxc::is_valid_dotted_identifier(".Abc.Def."));
    EXPECT_TRUE(jxc::is_valid_dotted_identifier("a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z"));
}


TEST(jxc_util, StringIntegerComparison)
{
    using jxc::detail::decimal_integer_string_less_than_or_equal;

    auto not_decimal_integer_string_less_than_or_equal = [](std::string_view lhs, std::string_view rhs)
    {
        return !decimal_integer_string_less_than_or_equal(lhs, rhs);
    };

    for (int32_t i = 0; i < 450; i++)
    {
        const std::string int_str = std::to_string(i);
        if (i <= 255)
        {
            EXPECT_TRUE(jxc::detail::value_lte_int_max<uint8_t>(int_str)) << jxc::format("Value {} fits in uint8_t", i);
        }
        else
        {
            EXPECT_FALSE(jxc::detail::value_lte_int_max<uint8_t>(int_str)) << jxc::format("Value {} does not fit in uint8_t", i);
        }
    }

    const int32_t uint16_max = static_cast<int32_t>(UINT16_MAX);
    for (int32_t i = 0; i <= uint16_max + 1000; i++)
    {
        const std::string int_str = std::to_string(i);
        if (i <= uint16_max)
        {
            EXPECT_TRUE(jxc::detail::value_lte_int_max<uint16_t>(int_str)) << jxc::format("Value {} fits in uint16_t", i);
        }
        else
        {
            EXPECT_FALSE(jxc::detail::value_lte_int_max<uint16_t>(int_str)) << jxc::format("Value {} too large to fit in uint16_t", i);
        }
    }

    EXPECT_PRED2(decimal_integer_string_less_than_or_equal, "99", "101");
    EXPECT_PRED2(decimal_integer_string_less_than_or_equal, "100", "101");
    EXPECT_PRED2(decimal_integer_string_less_than_or_equal, "101", "101");
    EXPECT_PRED2(not_decimal_integer_string_less_than_or_equal, "102", "101");
}


TEST(jxc_util, IntegerFitTests)
{
    using jxc::detail::unsigned_integer_value_fits_in_type;
    using jxc::detail::signed_integer_value_fits_in_type;

    // 0..255
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint8_t>(0));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint8_t>(64));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint8_t>(99));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint8_t>(199));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint8_t>(200));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint8_t>(255));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint8_t>(256));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint8_t>(500));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint8_t>(99999999));

    // 0..65535
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint16_t>(0));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint16_t>(64));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint16_t>(25123));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint16_t>(65534));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint16_t>(65535));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint16_t>(65536));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint16_t>(99999999));

    // 0..4294967295
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint32_t>(0));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint32_t>(64));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint32_t>(3123433123));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint32_t>(4294967294));
    EXPECT_TRUE(unsigned_integer_value_fits_in_type<uint32_t>(4294967295));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint32_t>(4294967296));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint32_t>(99999999999));
    EXPECT_FALSE(unsigned_integer_value_fits_in_type<uint32_t>(1234999999999));

    // -128..127
    EXPECT_FALSE(signed_integer_value_fits_in_type<int8_t>(-9999999));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int8_t>(-500));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int8_t>(-129));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int8_t>(-128));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int8_t>(-50));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int8_t>(0));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int8_t>(50));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int8_t>(127));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int8_t>(128));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int8_t>(500));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int8_t>(99999999));

    // -32768..32767
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(-99999999));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(-50000));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(-32769));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(-32768));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(-32767));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(-20000));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(-10000));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(-500));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(-1));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(0));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(1));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(500));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(10000));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(20000));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(32766));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int16_t>(32767));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(32768));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(32769));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(40000));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(50000));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int16_t>(999999999));
    
    // -2147483648..2147483647
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(-90000000000));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(-3000000000));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(-2147483650));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(-2147483649));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(-2147483648));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(-2147483647));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(-99999999));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(-5000));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(-1));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(0));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(1));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(5000));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(99999999));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(2147483645));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(2147483646));
    EXPECT_TRUE(signed_integer_value_fits_in_type<int32_t>(2147483647));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(2147483648));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(2147483649));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(3000000000));
    EXPECT_FALSE(signed_integer_value_fits_in_type<int32_t>(9000000000000));
}


enum class NotABitmaskEnum : uint8_t
{
    None = 0,
    A = 1 << 0,
    B = 1 << 1,
};

enum class BitmaskTest : uint8_t
{
    None = 0,
    A = 1 << 0,
    B = 1 << 1,
    C = 1 << 2,
    D = 1 << 3,
    E = 1 << 4,
};

JXC_BITMASK_ENUM(BitmaskTest);

TEST(jxc_util, BitmaskEnums)
{
    BitmaskTest val = BitmaskTest::A | BitmaskTest::B | BitmaskTest::C;

    // test comparing flags directly
    EXPECT_NE(val & BitmaskTest::A, BitmaskTest::None);

    // test comparing flags with the jxc::is_set helper
    EXPECT_TRUE(jxc::is_set(val, BitmaskTest::A));
    EXPECT_TRUE(jxc::is_set(val, BitmaskTest::B));
    EXPECT_TRUE(jxc::is_set(val, BitmaskTest::C));
    EXPECT_FALSE(jxc::is_set(val, BitmaskTest::D));
    EXPECT_FALSE(jxc::is_set(val, BitmaskTest::E));

    // removing and adding flags
    val &= ~BitmaskTest::B;
    val |= BitmaskTest::E;
    EXPECT_TRUE(jxc::is_set(val, BitmaskTest::A));
    EXPECT_FALSE(jxc::is_set(val, BitmaskTest::B));
    EXPECT_TRUE(jxc::is_set(val, BitmaskTest::C));
    EXPECT_FALSE(jxc::is_set(val, BitmaskTest::D));
    EXPECT_TRUE(jxc::is_set(val, BitmaskTest::E));

    // this should not compile because NotABitmaskEnum is not a bitmask
    //EXPECT_TRUE(jxc::is_set(NotABitmaskEnum::A, NotABitmaskEnum::A));
}
