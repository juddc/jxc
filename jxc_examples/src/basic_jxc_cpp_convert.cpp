#include <iostream>
#include <cassert>
#include <array>
#include <optional>
#include <unordered_map>
#include "jxc_cpp/jxc_cpp.h"


// Simple vector type
struct Vector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    friend auto operator<=>(const Vector3&, const Vector3&) = default;
};

JXC_DEFINE_STRUCT_CONVERTER(
    Vector3,
    jxc::def_struct<Vector3>("Vector3")
        .def_field("x", &Vector3::x, jxc::FieldFlags::Optional)
        .def_field("y", &Vector3::y, jxc::FieldFlags::Optional)
        .def_field("z", &Vector3::z, jxc::FieldFlags::Optional)
);


// Example type that does not have a default initializer
struct IDName
{
    uint64_t value = 0;
    std::string name;

    explicit IDName(uint64_t value, const char* name) : value(value), name(name) {}
    friend auto operator<=>(const IDName&, const IDName&) = default;
};

JXC_DEFINE_STRUCT_CONVERTER(
    IDName,
    jxc::def_struct<IDName>("id", jxc::StructFlags::AnnotationRequired | jxc::StructFlags::SingleLine)

        // specify how to construct an IDName, because the type does not have a default constructor
        .def_init([]() { return IDName(0, ""); })

        // custom serializer for this field - always serialize as a hexadecimal unsigned integer
        .def_field("value", &IDName::value, [](jxc::Serializer& doc, const IDName& value) { doc.value_uint_hex(value.value); })

        .def_field("name", &IDName::name)
);


void example_parse()
{
    // parse string map
    auto int_to_str_map = jxc::conv::parse<std::unordered_map<int32_t, std::optional<std::string>>>(R"(
        {
            -5: 'abc'
            0: 'neat'
            5: null
            22: "twenty-two"
        }
    )");
    assert(int_to_str_map.size() == 4);
    assert(int_to_str_map[-5] == "abc");
    assert(int_to_str_map[0] == "neat");
    assert(!int_to_str_map[5].has_value());
    assert(int_to_str_map[22] == "twenty-two");

    // parse custom struct
    assert(jxc::conv::parse<Vector3>("Vector3{ x: 1, y: 2, z: 3 }") == Vector3(1.0f, 2.0f, 3.0f));

    // parse custom struct wrapped in std::optional
    assert(jxc::conv::parse<std::optional<Vector3>>("Vector3{ x: -5, y: 5, z: 100 }").value() == Vector3(-5.0f, 5.0f, 100.0f));

    // parse custom struct wrapped in std::optional (null)
    assert(!jxc::conv::parse<std::optional<Vector3>>("null").has_value());

    // parse custom struct inside std::array
    std::array<Vector3, 4> vec_array = jxc::conv::parse<std::array<Vector3, 4>>(R"(
        std.array<Vector3, 4>[
            { x: 1, y: 2, z: 3 }
            { x: -5, y: 10, z: 15 }
            { x: -0.5, y: 1, z: 0 }
            { x: 0, y: -1, z: -1 }
        ]
    )");
    assert(vec_array.size() == 4);
    assert(vec_array[0] == Vector3(1.0f, 2.0f, 3.0f));
    assert(vec_array[1] == Vector3(-5.0f, 10.0f, 15.0f));
    assert(vec_array[2] == Vector3(-0.5f, 1.0f, 0.0f));
    assert(vec_array[3] == Vector3(0.0f, -1.0f, -1.0f));

    // parse IDName values:
    assert(jxc::conv::parse<IDName>("id{value:0,name:''}") == IDName(0, ""));
    assert(jxc::conv::parse<IDName>("id{value:12345,name:'abc'}") == IDName(12345,"abc"));

    // because we used the AnnotationRequired flag above, parsing the value without an annotation should throw parse_error:
    try
    {
        jxc::conv::parse<IDName>("{name:'abc'}");
    }
    catch (const jxc::parse_error& e)
    {
        assert(std::string(e.what()).find("Missing required annotation") != std::string::npos);
    }

    // Note that container converters like std::vector pass through their generic annotation to the parsers for each value, so
    // even when the annotation is required, you don't need to specify it for every single value when using it in an array or map.
    auto id_vec = jxc::conv::parse<std::vector<IDName>>(
        "vector<id>[{value:0x42, name:'abc'}, {value:0x43, name:'def'}, {value:0x44, name:'ghi'}]");
    std::vector<IDName> id_vec_expected{ IDName(66, "abc"), IDName(67, "def"), IDName(68, "ghi") };
    assert(id_vec == id_vec_expected);

    auto id_map = jxc::conv::parse<std::unordered_map<int32_t, IDName>>(
        "unordered_map<int32_t, id>{ 5: {value: 0x1, name:'abc'}, 20: {value: 0xFF, name:'def'}, -3: {value: 0xA, name:'ghi'} }");
    std::unordered_map<int32_t, IDName> id_map_expected{ {5, IDName(1, "abc")}, {20, IDName(255, "def")}, {-3, IDName(10, "ghi")} };
    assert(id_map == id_map_expected);
}

void example_serialize()
{
    std::cout << "=== Serialize string map ===\n";
    std::unordered_map<int32_t, std::optional<std::string>> int_to_str_map = {
        { -5, "abc" },
        { 0, "neat" },
        { 5, std::nullopt },
        { 22, "twenty-two" },
    };
    std::cout << jxc::conv::serialize(int_to_str_map) << "\n\n";

    std::cout << "=== Serialize custom struct ===\n";
    std::cout << jxc::conv::serialize(Vector3(1, 2, 3)) << "\n\n";

    std::cout << "=== Serialize custom struct wrapped in std::optional (with value) ===\n";
    std::cout << jxc::conv::serialize<std::optional<Vector3>>(Vector3(1, 2, 3)) << "\n\n";

    std::cout << "=== Serialize custom struct wrapped in std::optional (no value) ===\n";
    std::cout << jxc::conv::serialize<std::optional<Vector3>>(std::nullopt) << "\n\n";

    std::cout << "=== Serialize custom struct in std::vector ===\n";
    std::cout << jxc::conv::serialize(
        std::vector<Vector3>{
            Vector3(1, 2, 3),
            Vector3(0, 0, 0),
            Vector3(100, 200, -300.5),
        }
    ) << "\n";

    std::cout << "=== Serialize custom struct with SingleLine flag ===\n";
    std::cout << jxc::conv::serialize(IDName(999123, "name")) << "\n\n";

    std::cout << jxc::conv::serialize(std::vector<IDName>{ IDName(12345, "value1"), IDName(98765, "value2") }) << "\n\n";
}

int main(int argc, const char** argv)
{
    example_parse();
    example_serialize();
    return 0;
}

