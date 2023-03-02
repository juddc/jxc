#include <iostream>
#include <cassert>
#include <array>
#include <optional>
#include <unordered_map>
#include "jxc_cpp/jxc_cpp.h"


struct Vector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float value) : x(value), y(value), z(value) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    friend auto operator<=>(const Vector3&, const Vector3&) = default;
};


struct Quaternion
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    friend auto operator<=>(const Quaternion&, const Quaternion&) = default;
};


struct Transform
{
    Vector3 position = Vector3();
    Quaternion orientation = Quaternion();
    Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);

    Transform() = default;
    Transform(const Vector3& position, const Quaternion& orientation, const Vector3& scale)
        : position(position)
        , orientation(orientation)
        , scale(scale)
    {
    }
    friend auto operator<=>(const Transform&, const Transform&) = default;
};


// Fully custom converter for Vector3
template<>
struct jxc::Converter<Vector3>
{
    using value_type = Vector3;

    static const jxc::TokenList& get_annotation()
    {
        static const jxc::TokenList anno = jxc::TokenList::from_identifier("Vector3");
        return anno;
    }

    static void serialize(Serializer& doc, const Vector3& value)
    {
        doc.annotation("Vector3");
        if (value.x == 0 && value.y == 0 && value.z == 0)
        {
            // If all values are zero, serialize as the empty array
            doc.array_empty();
        }
        else if (value.x == value.y && value.x == value.z)
        {
            // If all values are equal, serialize as a single number
            doc.value_float(value.x);
        }
        else
        {
            // Otherwise, serialize all three values in an array.
            // NB. Specifying the separator string as ", " forces no line breaks,
            // even if line breaks are specified in the serializer settings.
            doc.array_begin(", ")
                .value_float(value.x)
                .value_float(value.y)
                .value_float(value.z)
                .array_end();
        }
    }

    static Vector3 parse(conv::Parser& parser, TokenView generic_anno)
    {
        if (TokenView anno = parser.get_value_annotation(generic_anno))
        {
            parser.require_annotation(anno, get_annotation());
        }

        // Convert the value to a Vector3 based on the element type
        switch (parser.value().type)
        {
        case jxc::ElementType::Null:
            // Allow "vec3 null" to convert to a zero vector
            return Vector3(0, 0, 0);

        case jxc::ElementType::Number:
            // Allow a plain number (eg. "vec3 123.456"), and treat that number as all three components
            return Vector3(parser.parse_value<float>());

        case jxc::ElementType::Bool:
            // Allow a bool (eg. "vec3 true"), and convert that bool to a float and set it to all three components
            return Vector3(static_cast<float>(parser.parse_value<bool>()));

        case jxc::ElementType::BeginArray:
        {
            // Allow an array, either empty or with three values

            // If the next element is EndArray, then this is an empty array
            parser.require_next();
            if (parser.value().type == jxc::ElementType::EndArray)
            {
                return Vector3();
            }

            // Otherwise, assume we're getting exactly three numbers
            Vector3 result;
            result.x = parser.parse_value<float>();
            parser.require_next();
            result.y = parser.parse_value<float>();
            parser.require_next();
            result.z = parser.parse_value<float>();
            parser.require_next();
            parser.require(jxc::ElementType::EndArray);
            return result;
        }

        case jxc::ElementType::BeginObject:
        {
            // Allow an object, and look for the keys x, y, and z
            Vector3 result;
            while (parser.next())
            {
                // if we get an EndObject element, then we're done
                if (parser.value().type == jxc::ElementType::EndObject)
                {
                    break;
                }

                // The next element must be an ObjectKey
                parser.require(jxc::ElementType::ObjectKey);
                
                // Save the key to a string variable so we can read it after we advance to the key's value
                const std::string key = std::string(parser.value().token.value.as_view());

                // Advance to the key's value
                parser.require_next();

                // Parse the value as a number, storing it in either x, y, or z, depending on the object key
                if (key == "x")
                {
                    result.x = parser.parse_value<float>();
                }
                else if (key == "y")
                {
                    result.y = parser.parse_value<float>();
                }
                else if (key == "z")
                {
                    result.z = parser.parse_value<float>();
                }
                else
                {
                    throw jxc::parse_error(jxc::format("Invalid key for Vector3: {}", key), parser.value());
                }
            }

            return result;
        }

        case jxc::ElementType::BeginExpression:
        {
            // Allow an expression (eg. "vec3(1 2 3)"), and parse it as three numbers
            float values[3] = { 0.0f, 0.0f, 0.0f };
            size_t value_index = 0;

            float multiplier = 1.0f;

            while (parser.next())
            {
                if (parser.value().type == jxc::ElementType::EndExpression)
                {
                    break;
                }

                if (value_index >= 3)
                {
                    throw jxc::parse_error("Vector3 requires exactly three values", parser.value());
                }
                else if (parser.value().type == jxc::ElementType::ExpressionToken)
                {
                    // the only token we support is Minus, to negate the next value
                    parser.require(jxc::TokenType::Minus);
                    multiplier *= -1.0f;
                }
                else if (parser.value().type == jxc::ElementType::Number)
                {
                    values[value_index] = parser.parse_value<float>() * multiplier;
                    value_index += 1;

                    // reset the multiplier for the next value
                    multiplier = 1.0f;
                }
            }

            if (value_index == 0)
            {
                // allow empty expression
                return Vector3();
            }
            else if (value_index == 1)
            {
                // allow expression with one value
                return Vector3(values[0]);
            }
            else if (value_index == 3)
            {
                // allow expression with three values
                return Vector3(values[0], values[1], values[2]);
            }

            // any other value count is not valid
            throw jxc::parse_error(jxc::format("Expected exactly 3 values, got {}", value_index + 1), parser.value());
        }

        default:
            throw jxc::parse_error(jxc::format("Invalid element type {} for Vector3",
                jxc::element_type_to_string(parser.value().type)),
                parser.value());
        }
    }
};


// Use an auto-converter for Quaternion
JXC_DEFINE_STRUCT_CONVERTER(
    Quaternion,
    jxc::def_struct<Quaternion>("Quaternion")
        .def_field("x", &Quaternion::x, jxc::FieldFlags::Optional)
        .def_field("y", &Quaternion::y, jxc::FieldFlags::Optional)
        .def_field("z", &Quaternion::z, jxc::FieldFlags::Optional)
        .def_field("w", &Quaternion::w, jxc::FieldFlags::Optional)
        .def_serialize_override([](jxc::Serializer& doc, const Quaternion& value)
        {
            // serialize as the empty object if the value is the identity quaternion
            if (value.x == 0 && value.y == 0 && value.z == 0 && value.w == 1)
            {
                doc.annotation("Quaternion").object_empty();
                return jxc::OverrideMode::PreventDefault;
            }
            // otherwise use the default serializer
            return jxc::OverrideMode::Default;
        })
        .def_parse_override([](jxc::conv::Parser& parser, TokenView generic_anno, Quaternion& out_value)
        {
            // if the value is an array, use this custom parse function
            if (parser.value().type == jxc::ElementType::BeginArray)
            {
                if (jxc::TokenView anno = parser.get_value_annotation(generic_anno))
                {
                    parser.require_identifier_annotation({ "Quaternion" });
                }

                parser.require_next();

                // allow empty array or exactly 4 numbers
                if (parser.value().type == jxc::ElementType::EndArray)
                {
                    out_value = Quaternion{};
                }
                else
                {
                    out_value.x = parser.parse_value<float>();
                    parser.require_next();
                    out_value.y = parser.parse_value<float>();
                    parser.require_next();
                    out_value.z = parser.parse_value<float>();
                    parser.require_next();
                    out_value.w = parser.parse_value<float>();
                    parser.require_next();
                    parser.require(jxc::ElementType::EndArray);
                }
                return jxc::OverrideMode::PreventDefault;
            }

            // otherwise use the default parse function
            return jxc::OverrideMode::Default;
        })
);


// Use an auto-converter for Transform
JXC_DEFINE_STRUCT_CONVERTER(
    Transform,
    jxc::def_struct<Transform>("Transform")
        .def_field("position", &Transform::position, jxc::FieldFlags::Optional)
        .def_field("orientation", &Transform::orientation, jxc::FieldFlags::Optional)
        .def_field("scale", &Transform::scale, jxc::FieldFlags::Optional)
);


void example_parse()
{
    // Test out the different ways we can parse our Vector3 type

    // null version:
    assert(jxc::conv::parse<Vector3>("Vector3 null") == Vector3());

    // number version:
    assert(jxc::conv::parse<Vector3>("Vector3 0") == Vector3(0));
    assert(jxc::conv::parse<Vector3>("Vector3 2.5") == Vector3(2.5f));

    // bool version:
    assert(jxc::conv::parse<Vector3>("Vector3 true") == Vector3(1));
    assert(jxc::conv::parse<Vector3>("Vector3 false") == Vector3(0));

    // list version:
    assert(jxc::conv::parse<Vector3>("Vector3[]") == Vector3());
    assert(jxc::conv::parse<Vector3>("Vector3[1, 2, 3]") == Vector3(1, 2, 3));

    // object version:
    assert(jxc::conv::parse<Vector3>("Vector3{ x: -1, y: -2.5, z: 4.25 }") == Vector3(-1.0f, -2.5f, 4.25f));

    // expression version:
    assert(jxc::conv::parse<Vector3>("Vector3()") == Vector3());
    assert(jxc::conv::parse<Vector3>("Vector3(2.5)") == Vector3(2.5f));
    assert(jxc::conv::parse<Vector3>("Vector3(-0.2 0.9 42)") == Vector3(-0.2f, 0.9f, 42.0f));

    // Parse Vector3 as part of another type

    // String to transform map:
    auto transform_map = jxc::conv::parse<std::unordered_map<std::string, std::optional<Transform>>>(R"(
        {
            object1: Transform{
                position: Vector3{}
                orientation: Quaternion{}
                scale: Vector3{}
            }
            object2: Transform{
                position: Vector3[5, 10, -15]
                orientation: { x: 0.0, y: 0.0, z: 0.0, w: 1.0 }
                scale: Vector3 1.0  # plain number value
            }
            object3: Transform{
                position: Vector3(-10 -20 -30)
                orientation: Quaternion[0.5, 0.25, 0.5, -0.6]
                scale: Vector3[ 2, 3, 1 ]
            }
            object4: null
        }
    )");
    assert(transform_map.size() == 4);
    assert(transform_map["object1"] == Transform(Vector3(0), Quaternion(), Vector3(0)));
    assert(transform_map["object2"] == Transform(Vector3(5, 10, -15), Quaternion(), Vector3(1.0)));
    assert(transform_map["object3"] == Transform(Vector3(-10, -20, -30), Quaternion(0.5f, 0.25f, 0.5f, -0.6f), Vector3(2, 3, 1)));
    assert(!transform_map["object4"].has_value());
}

void example_serialize()
{
    std::cout << "=== Serialize Vector3 ===\n";
    std::cout << jxc::conv::serialize(Vector3()) << "\n";
    std::cout << jxc::conv::serialize(Vector3(1)) << "\n";
    std::cout << jxc::conv::serialize(Vector3(-5.46f, 3.25f, 100.0f)) << "\n";

    std::cout << "=== Serialize string map ===\n";
    std::unordered_map<int32_t, std::tuple<std::string, Transform>> int_to_tuple_map = {
        { 2, std::make_tuple(std::string("object1"), Transform()) },
        { 42, std::make_tuple(std::string("object2"), Transform(Vector3(-10, -20, -30), Quaternion(0.5f, 0.22f, 1.8f, -1.2f), Vector3(2, 3, 1))) },
        { -20, std::make_tuple(std::string("object3"), Transform(Vector3(5, 10, -15), Quaternion(), Vector3(1))) },
    };
    std::cout << jxc::conv::serialize(int_to_tuple_map) << "\n\n";

    // You can mix jxc::Value with your own types to allow users to input any valid JXC value
    std::cout << "=== Serialize value map ===\n";
    std::unordered_map<jxc::Value, std::tuple<jxc::Value, Vector3>> any_to_tuple_map = {
        { 123, std::make_tuple("abc", Vector3()) },
        { nullptr, std::make_tuple(true, Vector3(1, 2, 3)) },
        { true, std::make_tuple(42, Vector3(-2.5f, 0.0f, 100.0f)) },
        { "dotted.key.*", std::make_tuple(jxc::annotated_array("value_list", { 1, 2, true, nullptr }), Vector3(0.0f, -5.0f, 42.0f)) },
    };
    std::cout << jxc::conv::serialize(any_to_tuple_map) << "\n\n";
}

int main(int argc, const char** argv)
{
    example_parse();
    example_serialize();
    return 0;
}

