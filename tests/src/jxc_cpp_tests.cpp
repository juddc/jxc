#include "jxc_cpp_tests.h"
#include "jxc/jxc_serializer.h"

#if 0
// minimal, no-whitespace serializer settings for ease of test string comparisons
static const jxc::SerializerSettings settings_minimal = {
    .pretty_print = false,
    .target_line_length = 0,
    .indent = "",
    .linebreak = "",
    .key_separator = ":",
    .value_separator = ",",
};


TEST(jxc_cpp_value, ValueObjectBasics)
{
    using jxc::Value;
    using jxc::ValueType;

    EXPECT_EQ(Value().get_type(), ValueType::Invalid);
    EXPECT_EQ(Value(jxc::tag::Invalid{}).get_type(), ValueType::Invalid);
    EXPECT_EQ(Value(jxc::default_invalid).get_type(), ValueType::Invalid);

    EXPECT_EQ(Value(nullptr).get_type(), ValueType::Null);
    EXPECT_EQ(Value(jxc::tag::Null{}).get_type(), ValueType::Null);
    EXPECT_EQ(Value(jxc::default_null).get_type(), ValueType::Null);

    EXPECT_EQ(Value(false).get_type(), ValueType::Bool);
    EXPECT_EQ(Value(true).get_type(), ValueType::Bool);
    EXPECT_EQ(Value(jxc::tag::Bool{}).get_type(), ValueType::Bool);
    EXPECT_EQ(Value(jxc::default_bool).get_type(), ValueType::Bool);

    EXPECT_EQ(Value(0).get_type(), ValueType::SignedInteger);
    EXPECT_EQ(Value(jxc::tag::Integer{}).get_type(), ValueType::SignedInteger);
    EXPECT_EQ(Value(jxc::default_integer).get_type(), ValueType::SignedInteger);
    EXPECT_EQ(Value(static_cast<int8_t>(0)).get_type(), ValueType::SignedInteger);
    EXPECT_EQ(Value(static_cast<int16_t>(0)).get_type(), ValueType::SignedInteger);
    EXPECT_EQ(Value(static_cast<int32_t>(0)).get_type(), ValueType::SignedInteger);
    EXPECT_EQ(Value(static_cast<int64_t>(0)).get_type(), ValueType::SignedInteger);

    EXPECT_EQ(Value(0u).get_type(), ValueType::UnsignedInteger);
    EXPECT_EQ(Value(static_cast<uint8_t>(0)).get_type(), ValueType::UnsignedInteger);
    EXPECT_EQ(Value(static_cast<uint16_t>(0)).get_type(), ValueType::UnsignedInteger);
    EXPECT_EQ(Value(static_cast<uint32_t>(0)).get_type(), ValueType::UnsignedInteger);
    EXPECT_EQ(Value(static_cast<uint64_t>(0)).get_type(), ValueType::UnsignedInteger);

    EXPECT_EQ(Value(0.0).get_type(), ValueType::Float);
    EXPECT_EQ(Value(0.0f).get_type(), ValueType::Float);
    EXPECT_EQ(Value(static_cast<float>(0)).get_type(), ValueType::Float);
    EXPECT_EQ(Value(static_cast<double>(0)).get_type(), ValueType::Float);
    EXPECT_EQ(Value(jxc::default_float).get_type(), ValueType::Float);
    EXPECT_EQ(Value(jxc::tag::Float{}).get_type(), ValueType::Float);

    EXPECT_EQ(Value("").get_type(), ValueType::String);
    EXPECT_EQ(Value(jxc::default_string).get_type(), ValueType::String);
    EXPECT_EQ(Value(jxc::tag::String{}).get_type(), ValueType::String);
    EXPECT_EQ(Value(std::string_view{}).get_type(), ValueType::String);
    EXPECT_EQ(Value(std::string{}).get_type(), ValueType::String);
    EXPECT_EQ(Value(jxc::FlexString{}).get_type(), ValueType::String);

    EXPECT_EQ(Value(std::vector<uint8_t>{}).get_type(), ValueType::Bytes);
    EXPECT_EQ(Value(jxc::BytesView{}).get_type(), ValueType::Bytes);
    EXPECT_EQ(Value(jxc::default_bytes).get_type(), ValueType::Bytes);
    EXPECT_EQ(Value(jxc::tag::Bytes{}).get_type(), ValueType::Bytes);

    EXPECT_EQ(Value(jxc::tag::Array{}).get_type(), ValueType::Array);
    EXPECT_EQ(Value(jxc::default_array).get_type(), ValueType::Array);
    EXPECT_EQ(Value{ 0 }.get_type(), ValueType::Array); // single-element array using initializer_list constructor
    EXPECT_EQ((Value{ 0u, true, 1.25 }.get_type()), ValueType::Array); // multi-element array using initializer_list constructor
    EXPECT_EQ(Value(jxc::tag::Object{}).get_type(), ValueType::Object);
    EXPECT_EQ(Value(jxc::default_object).get_type(), ValueType::Object);
}


TEST(jxc_cpp_value, ValueNumberBehavior)
{
    using jxc::Value;

    EXPECT_EQ(Value(123).get_type(), jxc::ValueType::SignedInteger);
    EXPECT_EQ(Value(-123).get_type(), jxc::ValueType::SignedInteger);
    EXPECT_EQ(Value(123u).get_type(), jxc::ValueType::UnsignedInteger);
    EXPECT_EQ(Value(123.0).get_type(), jxc::ValueType::Float);

    // equality
    Value val = 123;
    EXPECT_EQ(val, 123);
    EXPECT_EQ(val, val);

    // two Value objects with the same value and type are equal
    Value val2 = 123;
    EXPECT_EQ(val, val2);

    // different annotations means no longer equal
    val.set_annotation("int");
    EXPECT_NE(val, val2);

    // same annotation now, so they should be equal again
    val2.set_annotation("int");
    EXPECT_EQ(val, val2);

    // inverse operator
    val = -val;
    EXPECT_EQ(val, -123);
    EXPECT_NE(val, val2);
    EXPECT_EQ(val, -val2);

    Value val3 = 123u;
    val3.set_annotation("int");
    EXPECT_EQ(val3.get_type(), jxc::ValueType::UnsignedInteger);
    EXPECT_EQ(val2, val3);

    Value val4 = -val3;
    EXPECT_EQ(val4, -123);
    EXPECT_EQ(val4.get_type(), jxc::ValueType::SignedInteger);
    EXPECT_EQ(val, val4);

    // number + suffix constructor
    Value val5(25.3, "px");
    EXPECT_EQ(val5.to_string(), "25.3px");
}


TEST(jxc_cpp_value, ValueLiterals)
{
    using jxc::Value;
    using jxc::ValueType;
    using namespace jxc::value_literals;

    EXPECT_EQ((123_jxc).get_type(), ValueType::UnsignedInteger);
    EXPECT_EQ((-123_jxc).get_type(), ValueType::SignedInteger);
    EXPECT_EQ((0.45_jxc).get_type(), ValueType::Float);
    EXPECT_EQ(("abc"_jxc).get_type(), ValueType::String);
}


// test custom suffixes
JXC_DEFINE_LITERAL_NUMBER_SUFFIX(_px, "px");
JXC_DEFINE_LITERAL_NUMBER_SUFFIX(_rem, "rem");
JXC_DEFINE_LITERAL_NUMBER_SUFFIX(_pct, "%");


TEST(jxc_cpp_value, CustomNumberLiterals)
{
    using jxc::Value;
    
    {
        Value val = 50_px;
        EXPECT_EQ(val.as_integer(), 50);
        EXPECT_EQ(val.get_number_suffix(), "px");
        EXPECT_EQ(val.to_string(), "50px");
    }

    {
        Value val = -50_px;
        EXPECT_EQ(val.as_integer(), -50);
        EXPECT_EQ(val.get_number_suffix(), "px");
        EXPECT_EQ(val.to_string(), "-50px");
    }

    {
        Value val = 50.25_px;
        assert(val.as_float() == 50.25);
        EXPECT_EQ(val.as_float(), 50.25);
        EXPECT_EQ(val.get_number_suffix(), "px");
        EXPECT_EQ(val.to_string(), "50.25px");
    }

    {
        Value v1 = 50_px;
        Value v2 = 50_rem;
        EXPECT_NE(v1, v2);
        EXPECT_EQ(v1.get_number_suffix(), "px");
        EXPECT_EQ(v2.get_number_suffix(), "rem");
        EXPECT_EQ(v1.as_integer(), v2.as_integer());
    }

    {
        Value val = 25_pct;
        EXPECT_EQ(val.to_string(), "25%");
    }
}


TEST(jxc_cpp_value, SimpleParsingWithAnnotations)
{
    using jxc::Value;
    using jxc::Document;

    {
        jxc::Value val = jxc::annotated("int", 5, "px");
        EXPECT_EQ(val.to_string(), "int 5px");
    }

    {
        Value val = jxc::annotated("vec3", jxc::default_array);
        val.push_back(1.2);
        val.push_back(3.5);
        val.push_back(-5.2);
        EXPECT_EQ(val.to_string(settings_minimal), "vec3[1.2,3.5,-5.2]");
    }

    {
        Value val = jxc::annotated("vec3", jxc::default_object);
        val["x"] = 1.2;
        val["y"] = 3.5;
        val["z"] = -5.2;
        EXPECT_EQ(val.to_string(settings_minimal), "vec3{x:1.2,y:3.5,z:-5.2}");

        Document doc(val.to_string(settings_minimal));
        Value val2 = doc.parse();
        EXPECT_TRUE(val2.is_valid());
        EXPECT_EQ(val, val2);
    }
}

#endif
