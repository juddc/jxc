#pragma once
#include "jxc_cpp/jxc_converter.h"
#include "jxc_cpp/jxc_map.h"


JXC_BEGIN_NAMESPACE(jxc)


template<typename T>
using FieldSerializeFunc = std::function<void(Serializer&, const T&)>;

template<typename T>
using FieldParseFunc = std::function<void(conv::Parser&, const std::string&, T&)>;

enum class FieldFlags : uint8_t
{
    None = 0,

    // When parsing, don't throw a parse error if the field is missing
    Optional = 1 << 0,

    // When parsing, allow the field to occur multiple times
    AllowMultiple = 1 << 1,
};

JXC_BITMASK_ENUM(FieldFlags);


template<typename T>
struct FieldMetadata
{
    std::string name;
    std::string field_type_name;  // only used for error messsages
    size_t index = invalid_idx;
    FieldFlags flags = FieldFlags::None;
    FieldSerializeFunc<T> serialize_field;
    FieldParseFunc<T> parse_field;
    bool is_valid = false;

    FieldMetadata() = default;
    FieldMetadata(const FieldMetadata&) = default;
    FieldMetadata& operator=(const FieldMetadata&) = default;

    explicit FieldMetadata(const char* name)
        : name(name)
        , is_valid(true)
    {
    }
};


// callbacks for extra fields (fields not defined in the struct)
template<typename T>
struct ExtraFields
{
    FieldSerializeFunc<T> serialize;
    FieldParseFunc<T> parse;

    inline operator bool() const { return serialize || parse; }
};


enum class StructFlags : uint8_t
{
    None = 0,

    // When parsing, throw a parse_error if there is no annotation
    AnnotationRequired = 1 << 0,

    // Don't use line breaks when serializing the object
    SingleLine = 1 << 1,
};

JXC_BITMASK_ENUM(StructFlags);


enum class OverrideMode : uint8_t
{
    Default = 0,
    PreventDefault,
};


template<typename T>
using DefaultConstructFunc = std::function<T()>;


template<typename T>
using SerializeOverrideFunc = std::function<OverrideMode(Serializer& doc, const T& value)>;


template<typename T>
using ParseOverrideFunc = std::function<OverrideMode(conv::Parser&, TokenView, T&)>;


JXC_BEGIN_NAMESPACE(detail)


template<typename T>
DefaultConstructFunc<T> make_default_construct_func()
{
    if constexpr (std::is_default_constructible_v<T>)
    {
        return []() -> T { return T(); };
    }
    else
    {
        return nullptr;
    }
}


template<typename T, typename FieldType>
FieldSerializeFunc<T> make_default_serialize_func(FieldType T::* field_ptr)
{
    JXC_ASSERT(field_ptr != nullptr);

    if constexpr (ConverterWithSerialize<FieldType>)
    {
        return [field_ptr](Serializer& doc, const T& value)
        {
            Converter<FieldType>::serialize(doc, value.*field_ptr);
        };
    }
    else
    {
        return nullptr;
    }
}

template<typename T, typename FieldType>
FieldParseFunc<T> make_default_parse_func(FieldType T::* field_ptr)
{
    JXC_ASSERT(field_ptr != nullptr);

    if constexpr (ConverterWithParse<FieldType>)
    {
        return [field_ptr](conv::Parser& parser, const std::string& field_key, T& out_value)
        {
            out_value.*field_ptr = parser.parse_value<FieldType>();
        };
    }
    else
    {
        return nullptr;
    }
}

// Helper function for handling varargs in def_field()
template<typename T, typename ArgT>
void update_field(FieldMetadata<T>& field, ArgT&& arg)
{
    if constexpr (std::is_same_v<ArgT, FieldFlags>)
    {
        field.flags |= arg;
    }
    else if constexpr (std::is_same_v<ArgT, FieldSerializeFunc<T>> || std::is_convertible_v<ArgT, FieldSerializeFunc<T>>)
    {
        field.serialize_field = arg;
    }
    else if constexpr (std::is_same_v<ArgT, FieldParseFunc<T>> || std::is_convertible_v<ArgT, FieldParseFunc<T>>)
    {
        field.parse_field = arg;
    }
    else
    {
        []<bool Err = true>() { static_assert(!Err, "Invalid field argument type"); };
    }
}

// Helper function for handling varargs in def_extra()
template<typename T, typename ArgT>
void update_extra(ExtraFields<T>& field, ArgT&& arg)
{
    if constexpr (std::is_same_v<ArgT, FieldSerializeFunc<T>> || std::is_convertible_v<ArgT, FieldSerializeFunc<T>>)
    {
        field.serialize = arg;
    }
    else if constexpr (std::is_same_v<ArgT, FieldParseFunc<T>> || std::is_convertible_v<ArgT, FieldParseFunc<T>>)
    {
        field.parse = arg;
    }
    else
    {
        []<bool Err = true>() { static_assert(!Err, "Invalid extra field argument type"); };
    }
}

JXC_END_NAMESPACE(detail)


template<typename T>
class StructConverterMetadata
{
public:
    using struct_type = T;

private:
    TokenList struct_annotation;
    StructFlags flags = StructFlags::None;
    StringMap<FieldMetadata<struct_type>> fields;
    ExtraFields<struct_type> extra_field;
    std::vector<std::string> field_order;
    DefaultConstructFunc<struct_type> default_construct = detail::make_default_construct_func<struct_type>();
    SerializeOverrideFunc<struct_type> serialize_override;
    ParseOverrideFunc<struct_type> parse_override;
    size_t num_required_fields = 0;
    size_t num_optional_fields = 0;

    void add_field(const FieldMetadata<struct_type>& field)
    {
        JXC_ASSERT(field.name.size() > 0);
        JXC_ASSERTF(!fields.contains(field.name), "Duplicate field {}", detail::debug_string_repr(field.name));

        // found required field
        if (is_set(field.flags, FieldFlags::Optional))
        {
            ++num_optional_fields;
        }
        else
        {
            ++num_required_fields;
        }

        field_order.push_back(field.name);
        fields.insert_or_assign(field.name, field);
        fields[field.name].index = field_order.size() - 1;
    }

    void setup_fields(std::initializer_list<FieldMetadata<struct_type>> field_meta)
    {
        for (const FieldMetadata<struct_type>& field : field_meta)
        {
            add_field(field);
        }
        JXC_DEBUG_ASSERT(num_required_fields + num_optional_fields == field_order.size());
        JXC_DEBUG_ASSERT(num_required_fields + num_optional_fields == fields.size());
    }

    inline struct_type init_value() const
    {
        if (default_construct)
        {
            return default_construct();
        }

        if constexpr (std::is_default_constructible_v<struct_type>)
        {
            return struct_type();
        }
        else
        {
            JXC_ASSERTF(default_construct != nullptr && false,
                "Type {} is not default constructible, and does not have an initializer specified. Did you forget to call def_init()?",
                struct_annotation.source());
            return *reinterpret_cast<struct_type*>(0);
        }
    }

public:
    explicit StructConverterMetadata(std::string_view anno, StructFlags flags = StructFlags::None)
        : struct_annotation(TokenList::parse_annotation_checked(anno))
        , flags(flags)
    {
    }

    explicit StructConverterMetadata(TokenView anno, StructFlags flags = StructFlags::None)
        : struct_annotation(TokenList(anno))
        , flags(flags)
    {
    }

    inline const TokenList& get_annotation() const
    {
        return struct_annotation;
    }

    // begin builder methods

    template<typename Lambda>
    StructConverterMetadata& def_init(Lambda&& init_func)
    {
        default_construct = init_func;
        return *this;
    }

    template<typename FieldType, typename... TArgs>
    StructConverterMetadata& def_field(const char* name, FieldType T::* field_ptr, TArgs&&... args)
    {
        FieldMetadata<T> new_field(name);

        // update metadata based on argument types
        (detail::update_field<T>(new_field, std::forward<TArgs>(args)), ...);

        // set up default callbacks if we didn't get one as an argument
        if (!new_field.serialize_field)
        {
            new_field.serialize_field = detail::make_default_serialize_func<T>(field_ptr);
        }

        if (!new_field.parse_field)
        {
            new_field.parse_field = detail::make_default_parse_func<T>(field_ptr);
        }

        // get the field type name for use in error messages
        new_field.field_type_name = detail::get_type_name<FieldType>();

        // add the field
        add_field(new_field);
        return *this;
    }

    // Like a field, but not real. Requires a defined getter and setter.
    template<typename... TArgs>
    StructConverterMetadata& def_property(const char* name, TArgs&&... args)
    {
        FieldMetadata<T> new_prop(name);

        // update metadata based on argument types
        (detail::update_field<T>(new_prop, std::forward<TArgs>(args)), ...);

        JXC_ASSERTF(new_prop.serialize_field || new_prop.parse_field,
            "def_property({}) requires a parse function, a serialize function, or both",
            detail::debug_string_repr(name));
        
        // add the property
        add_field(new_prop);
        return *this;
    }

    template<typename... TArgs>
    StructConverterMetadata& def_extra(TArgs&&... args)
    {
        ExtraFields<T> result{};

        JXC_ASSERTF(!extra_field.parse && !extra_field.serialize,
            "def_extra() may only be used once for type {}", struct_annotation.source());

        // update metadata based on argument types
        (detail::update_extra<T>(result, std::forward<TArgs>(args)), ...);

        JXC_ASSERTF(result.serialize || result.parse,
            "def_extra() requires a parse function, a serialize function, or both");

        extra_field = result;
        return *this;
    }

    template<typename Lambda>
    StructConverterMetadata& def_serialize_override(Lambda&& func)
    {
        serialize_override = func;
        return *this;
    }

    template<typename Lambda>
    StructConverterMetadata& def_parse_override(Lambda&& func)
    {
        parse_override = func;
        return *this;
    }

    // end builder methods

    inline size_t field_count() const
    {
        return field_order.size();
    }

    inline size_t field_count_required() const
    {
        return num_required_fields;
    }

    inline size_t field_count_optional() const
    {
        return num_optional_fields;
    }

    inline const std::vector<std::string>& get_field_names() const
    {
        return field_order;
    }

    inline const StringMap<FieldMetadata<struct_type>>& get_fields() const
    {
        return fields;
    }

    inline const FieldMetadata<struct_type>* find(std::string_view field_name) const
    {
        auto iter = fields.find(field_name);
        if (iter != fields.end())
        {
            return &iter->second;
        }
        return nullptr;
    }

    void serialize(Serializer& doc, const T& value) const
    {
        if (serialize_override)
        {
            const OverrideMode mode = serialize_override(doc, value);
            if (mode == OverrideMode::PreventDefault)
            {
                return;
            }
        }

        if (struct_annotation)
        {
            struct_annotation.serialize(doc);
        }

        doc.object_begin(is_set(flags, jxc::StructFlags::SingleLine) ? std::string_view{", "} : std::string_view{});

        for (const auto& field_name : field_order)
        {
            const FieldMetadata<T>* field = find(field_name);
            JXC_ASSERT(field != nullptr);
            //TODO: determine if we want to throw an exception for a type with no serialize_field function
            if (field->serialize_field)
            {
                doc.identifier_or_string(field_name);
                doc.object_sep();
                field->serialize_field(doc, value);
            }
        }

        // if we have an extra serialize function defined, call that to add anything extra to the object
        if (extra_field.serialize)
        {
            extra_field.serialize(doc, value);
        }

        doc.object_end();
    }

    struct_type parse(conv::Parser& parser, TokenView generic_anno) const
    {
        struct_type result = init_value();

        if (parse_override)
        {
            const OverrideMode mode = parse_override(parser, generic_anno, result);
            if (mode == OverrideMode::PreventDefault)
            {
                return result;
            }
        }

        std::vector<bool> assigned_fields;
        assigned_fields.resize(fields.size(), false);

        parser.require(ElementType::BeginObject);

        // check the specified annotation
        if (TokenView this_anno = parser.get_value_annotation(generic_anno))
        {
            if (this_anno != struct_annotation)
            {
                throw parse_error(jxc::format("Expected annotation {}, got {}", struct_annotation, detail::debug_string_repr(this_anno.source().as_view())));
            }
        }
        else if (is_set(flags, StructFlags::AnnotationRequired))
        {
            throw parse_error(jxc::format("Missing required annotation {}", struct_annotation), parser.value());
        }

        // parse the struct fields
        while (parser.next())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }

            const std::string key = parser.parse_token_as_object_key<std::string>(parser.value().token);

            const FieldMetadata<T>* field = find(key);
            bool use_extra = false;
            if (field == nullptr)
            {
                // if this struct has a handler for extra fields, use that
                if (extra_field.parse)
                {
                    use_extra = true;
                }
                else
                {
                    throw parse_error(jxc::format("Type {} has no such field {}", struct_annotation, key), parser.value());
                }
            }
            else if (!field->parse_field)
            {
                throw parse_error(jxc::format("Field {}::{} (type: {}) has no parse function", struct_annotation, key, field->field_type_name), parser.value());
            }
            else if (!is_set(field->flags, FieldFlags::AllowMultiple) && assigned_fields[field->index])
            {
                throw parse_error(jxc::format("Duplicate field {}::{}", struct_annotation, key), parser.value());
            }

            // advance to field value
            if (!parser.next())
            {
                throw parse_error("Unexpected end of stream");
            }

            JXC_DEBUG_ASSERT(field != nullptr || use_extra);

            if (field)
            {
                field->parse_field(parser, key, result);
                if (field->index < assigned_fields.size())
                {
                    assigned_fields[field->index] = true;
                }
            }
            else if (use_extra)
            {
                extra_field.parse(parser, key, result);
            }
        }

        // make sure we're not missing any non-optional fields
        if (field_count_optional() < field_count())
        {
            for (const auto& pair : get_fields())
            {
                const FieldMetadata<T>& field = pair.second;
                if (!jxc::is_set(field.flags, FieldFlags::Optional) && !assigned_fields[field.index])
                {
                    throw parse_error(jxc::format("Type {} is missing field {}", struct_annotation, field.name), parser.value());
                }
            }
        }

        return result;
    }
};



template<typename T>
StructConverterMetadata<T> def_struct(std::string_view anno, jxc::StructFlags flags = jxc::StructFlags::None)
{
    return StructConverterMetadata<T>(anno, flags);
}


template<typename T>
StructConverterMetadata<T> def_struct(const TokenList& anno, jxc::StructFlags flags = jxc::StructFlags::None)
{
    return StructConverterMetadata<T>(anno, flags);
}


//
// Defines a jxc::Converter for a given type, automatically generating parse and serialize functions
// for the fields in that type.
// The second argument to this macro should be an expression that returns a StructConverterMetadata
// object. The simplest way to do this is to call jxc::def_struct(). Note that StructConverterMetadata
// contains chainable methods for adding fields, flags, and function overrides.
//
// Example usage:
//
// struct Vec3
// {
//     float x = 0.0f;
//     float y = 0.0f;
//     float z = 0.0f;
// };
// JXC_DEFINE_STRUCT_CONVERTER(Vec3,
//     jxc::def_struct<Vec3>("vec3")
//         .def_field("x", &Vec3::x)
//         .def_field("y", &Vec3::y)
//         .def_field("z", &Vec3::z));
//
#define JXC_DEFINE_STRUCT_CONVERTER(CPP_TYPE, DEF_STRUCT_EXPR) \
    template<> \
    struct jxc::Converter<CPP_TYPE> { \
        using value_type = CPP_TYPE; \
        static const ::jxc::StructConverterMetadata<value_type>& fields() { \
            static const ::jxc::StructConverterMetadata<value_type> data = (DEF_STRUCT_EXPR);\
            return data; \
        } \
        static const ::jxc::TokenList& get_annotation() { \
            static const ::jxc::TokenList anno = fields().get_annotation(); \
            return anno; \
        } \
        static void serialize(::jxc::Serializer& doc, const value_type& value) { \
            fields().serialize(doc, value); \
        } \
        static value_type parse(::jxc::conv::Parser& parser, ::jxc::TokenView generic_anno) { \
            return fields().parse(parser, generic_anno); \
        } \
    }


JXC_END_NAMESPACE(jxc)
