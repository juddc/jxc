#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <stdexcept>
#include <assert.h>

#include "jxc/jxc.h"


namespace CustomParser
{

// base types
struct invalid_t {};
struct null_t {};
enum class boolean_t { False = 0, True = 1 };
using integer_t = int32_t;
using float_t = float;
using string_t = std::string;
using bytes_t = std::vector<uint8_t>;
using datetime_t = jxc::DateTime;

// array type
template<typename T>
using array_t = std::vector<T>;

// object key type
using base_object_key_t = std::variant<null_t, boolean_t, integer_t, string_t, bytes_t>;

struct object_key_t : base_object_key_t
{
    using base_object_key_t::base_object_key_t; // inherit constructors
    object_key_t(std::string_view str) : base_object_key_t(std::string(str)) {}

    bool operator==(const object_key_t& rhs) const
    {
        if (index() == rhs.index())
        {
            switch (index())
            {
            case 0: return true; // null
            case 1: return std::get<boolean_t>(*this) == std::get<boolean_t>(rhs);
            case 2: return std::get<integer_t>(*this) == std::get<integer_t>(rhs);
            case 3: return std::get<string_t>(*this) == std::get<string_t>(rhs);
            case 4: return std::get<bytes_t>(*this) == std::get<bytes_t>(rhs);
            default: break;
            }
        }
        return false;
    }

    bool operator!=(const object_key_t& rhs) const
    {
        return !operator==(rhs);
    }

    void serialize(jxc::Serializer& doc) const
    {
        switch (index())
        {
        case 0:
            doc.value_null();
            break;
        case 1:
            doc.value_bool(static_cast<bool>(std::get<boolean_t>(*this)));
            break;
        case 2:
            doc.value_int(static_cast<int64_t>(std::get<integer_t>(*this)));
            break;
        case 3:
        {
            const auto& str = std::get<string_t>(*this);
            doc.identifier_or_string(std::string_view(str.data(), str.size()));
            break;
        }
        case 4:
        {
            const auto& bytes = std::get<bytes_t>(*this);
            doc.value_bytes(jxc::BytesView(bytes.data(), bytes.size()));
            break;
        }
        default:
            assert(false && "Unexpected object key variant type");
            break;
        }
    }
};

inline size_t hash_object_key(const object_key_t& key);

} // namespace CustomParser

// std::hash overload for object_key_t needs to be in the global namespace and defined before object_t
template<>
struct std::hash<CustomParser::object_key_t>
{
    inline size_t operator()(const CustomParser::object_key_t& key) const
    {
        return CustomParser::hash_object_key(key);
    }
};

namespace CustomParser
{

class StringOutputBuffer : public jxc::IOutputBuffer
{
    std::vector<char> buffer;

public:
    virtual ~StringOutputBuffer()
    {
    }

    void write(const char* value, size_t value_len) override
    {
        size_t start_idx = buffer.size();
        buffer.resize(buffer.size() + value_len);
        for (size_t i = 0; i < value_len; i++)
        {
            buffer[start_idx + i] = value[i];
        }
    }

    void clear() override
    {
        buffer.clear();
    }

    string_t str() const
    {
        return string_t(buffer.begin(), buffer.end());
    }
};

// object type
template<typename T>
using object_t = std::unordered_map<object_key_t, T>;

// value type

// Unfortunately std::variant does not allow self-referential types, but we can get around this by making it a template
template<typename T>
using base_value_t = std::variant<
    invalid_t,
    null_t,
    boolean_t,
    integer_t,
    float_t,
    string_t,
    bytes_t,
    datetime_t,
    std::unique_ptr<array_t<T>>,
    std::unique_ptr<object_t<T>>
>;

// must be 1:1 with base_value_t types
enum class ValueType : uint8_t
{
    Invalid = 0,
    Null,
    Boolean,
    Integer,
    Float,
    String,
    Bytes,
    DateTime,
    Array,
    Object,
};

struct value_t : base_value_t<value_t>
{
    // inherit constructors
    using base_value_t<value_t>::base_value_t;

    static std::unique_ptr<array_t<value_t>> make_array() { return std::make_unique<array_t<value_t>>(); }
    static std::unique_ptr<object_t<value_t>> make_object() { return std::make_unique<object_t<value_t>>(); }

    inline ValueType get_type() const { return static_cast<ValueType>(index()); }

    inline bool is_valid() const { return get_type() != ValueType::Invalid; }
    inline bool is_invalid() const { return get_type() == ValueType::Invalid; }
    inline bool is_null() const { return get_type() == ValueType::Null; }
    inline bool is_boolean() const { return get_type() == ValueType::Boolean; }
    inline bool is_integer() const { return get_type() == ValueType::Integer; }
    inline bool is_float() const { return get_type() == ValueType::Float; }
    inline bool is_string() const { return get_type() == ValueType::String; }
    inline bool is_bytes() const { return get_type() == ValueType::Bytes; }
    inline bool is_datetime() const { return get_type() == ValueType::DateTime; }
    inline bool is_array() const { return get_type() == ValueType::Array; }
    inline bool is_object() const { return get_type() == ValueType::Object; }

    template<typename T>
    T* try_get()
    {
        // for array and object, auto-unpack their unique_ptrs
        if constexpr (std::is_same_v<T, array_t<value_t>> || std::is_same_v<T, object_t<value_t>>)
        {
            auto* result = std::get_if<std::unique_ptr<T>>(this);
            return (result != nullptr) ? result->get() : nullptr;
        }
        else
        {
            return std::get_if<T>(this);
        }
    }

    template<typename T>
    const T* try_get() const { return const_cast<value_t*>(this)->try_get<T>(); }

    template<typename T>
    T& as() { T* result = try_get<T>(); assert(result != nullptr); return *result; }

    template<typename T>
    const T& as() const { return const_cast<value_t*>(this)->as<T>(); }

    void serialize(jxc::Serializer& doc) const
    {
        switch (get_type())
        {
        case ValueType::Invalid:
            doc.annotation("invalid").expression_empty();
            break;
        case ValueType::Null:
            doc.value_null();
            break;
        case ValueType::Boolean:
            doc.value_bool(as<boolean_t>() == boolean_t::True);
            break;
        case ValueType::Integer:
            doc.value_int(static_cast<int64_t>(as<integer_t>()));
            break;
        case ValueType::Float:
            doc.value_float(static_cast<double>(as<float_t>()));
            break;
        case ValueType::String:
        {
            const auto& str = as<string_t>();
            doc.value_string(std::string_view(str.data(), str.size()));
            break;
        }
        case ValueType::Bytes:
        {
            const auto& bytes = as<bytes_t>();
            doc.value_bytes(jxc::BytesView(bytes.data(), bytes.size()));
            break;
        }
        case ValueType::DateTime:
        {
            const bool auto_strip_time = true;
            doc.value_datetime(as<datetime_t>(), auto_strip_time);
            break;
        }
        case ValueType::Array:
        {
            const auto& arr = as<array_t<value_t>>();
            doc.array_begin();
            for (const auto& item : arr)
            {
                item.serialize(doc);
            }
            doc.array_end();
            break;
        }
        case ValueType::Object:
        {
            const auto& obj = as<object_t<value_t>>();
            doc.object_begin();
            for (const auto& pair : obj)
            {
                pair.first.serialize(doc);
                doc.object_sep();
                pair.second.serialize(doc);
            }
            doc.object_end();
            break;
        }
        default:
            assert(false && "Unexpected value type");
            break;
        }
    }

    string_t to_jxc(const jxc::SerializerSettings& settings = jxc::SerializerSettings()) const
    {
        StringOutputBuffer result;
        jxc::Serializer doc(&result, settings);
        serialize(doc);
        doc.flush();
        return result.str();
    }
};

class Parser
{
private:
    std::string jxc_buffer;
    jxc::JumpParser parser;
    jxc::ErrorInfo parse_error;

public:
    Parser(std::string_view jxc_data)
        : jxc_buffer(std::string(jxc_data))
        , parser(jxc_buffer)
    {
    }

    bool has_error() const
    {
        return parse_error.is_err || parser.has_error();
    }

    const jxc::ErrorInfo& get_error() const
    {
        return parse_error.is_err ? parse_error : parser.get_error();
    }

    bool advance()
    {
        if (parser.next())
        {
            return true;
        }
        else if (parser.get_error().is_err)
        {
            throw std::runtime_error(parser.get_error().to_string(jxc_buffer));
        }
        return false;
    }

    value_t parse()
    {
        if (advance())
        {
            return parse_value(parser.value());
        }
        return invalid_t{};
    }

    value_t parse_value(const jxc::Element& ele)
    {
        switch (ele.type)
        {
        case jxc::ElementType::Null: return null_t{};
        case jxc::ElementType::Bool: return (ele.token.type == jxc::TokenType::True) ? boolean_t::True : boolean_t::False;
        case jxc::ElementType::Number: return parse_number_element(ele);
        case jxc::ElementType::String: return parse_string_element(ele);
        case jxc::ElementType::Bytes: return parse_bytes_element(ele);
        case jxc::ElementType::DateTime: return parse_datetime_element(ele);
        case jxc::ElementType::BeginArray: return parse_array();
        case jxc::ElementType::BeginExpression: return parse_expression();
        case jxc::ElementType::BeginObject: return parse_object();
        default:
            break;
        }
        throw std::runtime_error(std::string("Unexpected element type for value: ") + jxc::element_type_to_string(ele.type));
        return invalid_t{};
    }

    value_t parse_number_element(const jxc::Element& ele, bool allow_float = true)
    {
        assert(ele.type == jxc::ElementType::Number);

        jxc::util::NumberTokenSplitResult number;
        if (!jxc::util::split_number_token_value(ele.token, number, parse_error))
        {
            throw std::runtime_error(parse_error.to_string(jxc_buffer));
        }

        const bool is_int = number.exponent >= 0 && number.value.find_first_of('.') == std::string_view::npos;
        if (is_int)
        {
            integer_t int_value = 0;
            if (jxc::util::parse_number<integer_t>(ele.token, int_value, number, parse_error))
            {
                return int_value;
            }
            // parse_number sets parse_error when it returns false
            throw std::runtime_error(parse_error.to_string(jxc_buffer));
        }
        else
        {
            float_t float_value = 0;
            if (jxc::util::parse_number<float_t>(ele.token, float_value, number, parse_error))
            {
                return float_value;
            }
            // parse_number sets parse_error when it returns false
            throw std::runtime_error(parse_error.to_string(jxc_buffer));
        }
    }

    string_t parse_string_element(const jxc::Element& ele)
    {
        assert(ele.type == jxc::ElementType::String);

        // First, determine the max length this string could be to determine how large a buffer is needed to parse it.
        const size_t max_result_len = jxc::util::get_string_required_buffer_size(ele.token.value.as_view());

        // Make a buffer large enough for the final string
        string_t result;
        result.resize(max_result_len);

        // Now parse the string into the buffer
        size_t actual_result_len = 0;
        if (jxc::util::parse_string_token_to_buffer(ele.token, result.data(), result.size(), actual_result_len, parse_error))
        {
            // trim the result string to the actual parsed length before returning it
            result.resize(actual_result_len);
            return result;
        }
        else
        {
            // parse_string_token sets parse_error when it returns false
            throw std::runtime_error(parse_error.to_string(jxc_buffer));
        }
    }

    bytes_t parse_bytes_element(const jxc::Element& ele)
    {
        assert(ele.type == jxc::ElementType::Bytes);
        
        bytes_t result{};
        std::string_view raw_bytes_value = ele.token.value.as_view();
        const size_t max_num_bytes = jxc::util::get_byte_buffer_required_size(raw_bytes_value.data(), raw_bytes_value.size());
        result.resize(max_num_bytes, 0);

        size_t actual_num_bytes = 0;
        if (jxc::util::parse_bytes_token(ele.token, result.data(), result.size(), actual_num_bytes, parse_error))
        {
            result.resize(actual_num_bytes);
            return result;
        }
        else
        {
            // parse_bytes_token sets parse_error when it returns false
            throw std::runtime_error(parse_error.to_string(jxc_buffer));
        }
    }

    datetime_t parse_datetime_element(const jxc::Element& ele)
    {
        assert(ele.type == jxc::ElementType::DateTime);

        datetime_t result{};
        if (!jxc::util::parse_datetime_token(ele.token, result, parse_error))
        {
            // parse_datetime_token sets parse_error when it returns false
            throw std::runtime_error(parse_error.to_string(jxc_buffer));
        }
        return result;
    }

    value_t parse_array()
    {
        assert(parser.value().type == jxc::ElementType::BeginArray);
        auto result = value_t::make_array();
        while (advance())
        {
            const jxc::Element& ele = parser.value();
            if (ele.type == jxc::ElementType::EndArray)
            {
                break;
            }
            result->push_back(parse_value(ele));
        }
        return result;
    }

    value_t parse_expression_item(const jxc::Element& ele)
    {
        switch (ele.type)
        {
        case jxc::ElementType::Null: return null_t{};
        case jxc::ElementType::Bool: return (ele.token.type == jxc::TokenType::True) ? boolean_t::True : boolean_t::False;
        case jxc::ElementType::Number: return parse_number_element(ele);
        case jxc::ElementType::String: return parse_string_element(ele);
        case jxc::ElementType::Bytes: return parse_bytes_element(ele);
        case jxc::ElementType::DateTime: return parse_datetime_element(ele);
        case jxc::ElementType::ExpressionToken: return string_t(ele.token.value.data(), ele.token.value.size());
        case jxc::ElementType::Comment: return string_t(ele.token.value.data(), ele.token.value.size());
        default:
            break;
        }
        assert(false && "Invalid element type for expression value");
        return invalid_t{};
    }

    value_t parse_expression()
    {
        assert(parser.value().type == jxc::ElementType::BeginExpression);

        auto result = value_t::make_array();
        while (advance())
        {
            const jxc::Element& ele = parser.value();
            if (ele.type == jxc::ElementType::EndExpression)
            {
                break;
            }
            result->push_back(parse_expression_item(ele));
        }
        return result;
    }

    object_key_t parse_object_key(const jxc::Element& ele)
    {
        assert(ele.type == jxc::ElementType::ObjectKey);
        switch (ele.token.type)
        {
        case jxc::TokenType::Null: return null_t{};
        case jxc::TokenType::False: return boolean_t::False;
        case jxc::TokenType::True: return boolean_t::True;
        case jxc::TokenType::Identifier: return string_t(ele.token.value.data(), ele.token.value.size());
        case jxc::TokenType::String: return parse_string_element(ele);
        case jxc::TokenType::ByteString: return parse_bytes_element(ele);
        case jxc::TokenType::Number:
        {
            value_t number_val = parse_number_element(ele);
            if (number_val.is_float())
            {
                throw std::runtime_error("Got floating point value for object key");
            }
            return number_val.as<integer_t>();
        }
        default:
            break;
        }
        throw std::runtime_error("Unexpected object key token type");
    }

    value_t parse_object()
    {
        assert(parser.value().type == jxc::ElementType::BeginObject);
        auto result = value_t::make_object();
        while (advance())
        {
            const jxc::Element& key_ele = parser.value();
            if (key_ele.type == jxc::ElementType::EndObject)
            {
                break;
            }
            object_key_t key = parse_object_key(key_ele);
            if (!advance())
            {
                break;
            }
            result->insert_or_assign(key, parse_value(parser.value()));
        }
        return result;
    }
};


size_t hash_object_key(const object_key_t& key)
{
    auto hash_bytes = [](const bytes_t& bytes) -> size_t
    {
        size_t bytes_hash = 0;
        jxc::detail::hash_combine(bytes_hash, bytes.size());
        for (size_t i = 0; i < bytes.size(); i++)
        {
            jxc::detail::hash_combine(bytes_hash, static_cast<size_t>(bytes[i]));
        }
        return bytes_hash;
    };

    size_t key_hash = 0;
    jxc::detail::hash_combine(key_hash, key.index()); // include the variant type in the hash
    switch (key.index())
    {
    case 0: break; // null
    case 1: jxc::detail::hash_combine(key_hash, static_cast<size_t>(std::get<boolean_t>(key))); break;
    case 2: jxc::detail::hash_combine(key_hash, static_cast<size_t>(std::get<integer_t>(key))); break;
    case 3: jxc::detail::hash_combine(key_hash, std::hash<std::string>()(std::get<string_t>(key))); break;
    case 4: jxc::detail::hash_combine(key_hash, hash_bytes(std::get<bytes_t>(key))); break;
    default: assert(false && "invalid object key type"); break;
    }
    return key_hash;
}

} // namespace CustomParser


namespace test
{
template<typename LhsT, typename RhsT>
void assert_equal(const char* lhs_str, const char* rhs_str, const LhsT& lhs, const RhsT& rhs)
{
    const bool result = lhs == rhs;
    if (!result)
    {
        std::cerr << "Assertion failed: " << lhs_str << " (" << lhs << ") != " << rhs_str << " (" << rhs << ")\n";
        std::abort();
    }
}
void assert_true(const char* expr_str, bool result)
{
    if (!result)
    {
        std::cerr << "Assertion failed: " << expr_str << "\n";
        std::abort();
    }
}
#define ASSERT_EQUAL(LHS, RHS) test::assert_equal(#LHS, #RHS, (LHS), (RHS))
#define ASSERT_TRUE(EXPR) test::assert_true(#EXPR, !!(EXPR))
}


int main(int argc, const char** argv)
{
    using namespace CustomParser;

    // parse null
    {
        Parser parser("null");
        value_t value = parser.parse();
        ASSERT_TRUE(value.is_null());
        ASSERT_EQUAL(value.to_jxc(), "null");
    }

    // parse an integer
    {
        Parser parser("-991234");
        value_t value = parser.parse();
        ASSERT_TRUE(value.is_integer());
        ASSERT_EQUAL(value.as<integer_t>(), -991234);
        ASSERT_EQUAL(value.to_jxc(), "-991234");
    }

    // parse a byte array
    {
        Parser parser("b64'( Cr9s n/8= )'");
        value_t value = parser.parse();
        ASSERT_TRUE(value.is_bytes());
        const auto& bytes = value.as<bytes_t>();
        ASSERT_EQUAL(bytes.size(), 5);
        ASSERT_EQUAL(bytes[0], 0x0a);
        ASSERT_EQUAL(bytes[1], 0xbf);
        ASSERT_EQUAL(bytes[2], 0x6c);
        ASSERT_EQUAL(bytes[3], 0x9f);
        ASSERT_EQUAL(bytes[4], 0xff);

        jxc::SerializerSettings settings{};
        ASSERT_EQUAL(value.to_jxc(settings), "b64\"Cr9sn/8=\"");
    }

    // parse a more complex value
    {
        Parser parser("[0, 1, true, null, 1.75, {q: -10.75}, dt'2023-06-15']");
        value_t value = parser.parse();
        ASSERT_EQUAL(value.to_jxc(jxc::SerializerSettings::make_compact()), "[0,1,true,null,1.75,{q:-10.75},dt\"2023-06-15\"]");
        ASSERT_TRUE(value.is_array());
        auto& arr = value.as<array_t<value_t>>();
        ASSERT_EQUAL(arr.size(), 7);
        ASSERT_TRUE(arr[0].is_integer() && arr[0].as<integer_t>() == 0);
        ASSERT_TRUE(arr[1].is_integer() && arr[1].as<integer_t>() == 1);
        ASSERT_TRUE(arr[2].is_boolean() && static_cast<bool>(arr[2].as<boolean_t>()) == true);
        ASSERT_TRUE(arr[3].is_null());
        ASSERT_TRUE(arr[4].is_float() && arr[4].as<float_t>() == 1.75f);
        ASSERT_TRUE(arr[5].is_object());
        auto& obj = arr[5].as<object_t<value_t>>();
        ASSERT_EQUAL(obj.size(), 1);
        ASSERT_TRUE(obj.contains("q"));
        ASSERT_TRUE(obj["q"].is_float());
        ASSERT_EQUAL(obj["q"].as<float_t>(), -10.75f);
        ASSERT_TRUE(arr[6].is_datetime());
        ASSERT_EQUAL(arr[6].as<datetime_t>(), datetime_t(2023, 6, 15));
    }

    // construct a value and serialize it
    {
        value_t value = value_t::make_array();
        value.as<array_t<value_t>>().push_back(1234);
        value.as<array_t<value_t>>().push_back(boolean_t::True);
        value.as<array_t<value_t>>().push_back(null_t{});
        value.as<array_t<value_t>>().push_back(value_t::make_array());
        value.as<array_t<value_t>>().push_back(value_t::make_object());
        ASSERT_EQUAL(value.to_jxc(jxc::SerializerSettings::make_compact()), "[1234,true,null,[],{}]");
    }

    jxc::print("All tests passed successfully.\n");

    return 0;
}
