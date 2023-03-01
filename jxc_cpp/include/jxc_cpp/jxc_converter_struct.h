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


enum class StructFlags : uint8_t
{
    None = 0,

    // When parsing, throw a parse_error if there is no annotation
    AnnotationRequired = 1 << 1,
};

JXC_BITMASK_ENUM(StructFlags);


template<typename T>
class StructConverterMetadata
{
public:
    using struct_type = T;
    static_assert(std::is_default_constructible_v<struct_type>, "Type must be default-constructible");

private:
    TokenList annotation;
    StructFlags flags = StructFlags::None;
    StringMap<FieldMetadata<struct_type>> fields;
    FieldMetadata<struct_type> extra_field;
    std::vector<std::string> field_order;
    size_t num_required_fields = 0;
    size_t num_optional_fields = 0;

    void setup_fields(std::initializer_list<FieldMetadata<struct_type>> field_meta)
    {
        for (const FieldMetadata<struct_type>& field : field_meta)
        {
            // if we get a field with no name, that indicates this is the "extra" field, called on unknown keys
            if (field.name.size() == 0)
            {
                JXC_ASSERTF(!extra_field.is_valid, "More than one extra field specified for type {}", detail::debug_string_repr(field.name));
                extra_field = field;
                continue;
            }

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

        JXC_DEBUG_ASSERT(num_required_fields + num_optional_fields == field_order.size());
        JXC_DEBUG_ASSERT(num_required_fields + num_optional_fields == fields.size());
    }

public:
    StructConverterMetadata(std::string_view struct_annotation, std::initializer_list<FieldMetadata<struct_type>> field_meta, StructFlags flags = StructFlags::None)
        : annotation(struct_annotation)
        , flags(flags)
    {
        setup_fields(field_meta);
    }

    StructConverterMetadata(const TokenList& struct_annotation, std::initializer_list<FieldMetadata<struct_type>> field_meta, StructFlags flags = StructFlags::None)
        : annotation(struct_annotation)
        , flags(flags)
    {
        setup_fields(field_meta);
    }

    StructConverterMetadata(TokenList&& struct_annotation, std::initializer_list<FieldMetadata<struct_type>> field_meta, StructFlags flags = StructFlags::None)
        : annotation(std::move(struct_annotation))
        , flags(flags)
    {
        setup_fields(field_meta);
    }

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
        if (annotation)
        {
            annotation.serialize(doc);
        }

        doc.object_begin();

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
        if (extra_field.is_valid && extra_field.serialize_field)
        {
            extra_field.serialize_field(doc, value);
        }

        doc.object_end();
    }

    struct_type parse(conv::Parser& parser, TokenView generic_anno) const
    {
        struct_type result = {};

        std::vector<bool> assigned_fields;
        assigned_fields.resize(fields.size(), false);

        parser.require(ElementType::BeginObject);

        // check the specified annotation
        if (TokenView this_anno = parser.get_value_annotation(generic_anno))
        {
            if (this_anno != annotation)
            {
                throw parse_error(jxc::format("Expected annotation {}, got {}", annotation, detail::debug_string_repr(this_anno.source().as_view())));
            }
        }
        else if (is_set(flags, StructFlags::AnnotationRequired))
        {
            throw parse_error(jxc::format("Missing required annotation {}", annotation), parser.value());
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
            if (field == nullptr)
            {
                // if this struct has a handler for extra fields, use that
                if (extra_field.is_valid && extra_field.parse_field)
                {
                    field = &extra_field;
                }
                else
                {
                    throw parse_error(jxc::format("Type {} has no such field {}", annotation, key), parser.value());
                }
            }
            else if (!field->parse_field)
            {
                throw parse_error(jxc::format("Field {}::{} (type: {}) has no parse function", annotation, key, field->field_type_name), parser.value());
            }
            else if (!is_set(field->flags, FieldFlags::AllowMultiple) && assigned_fields[field->index])
            {
                throw parse_error(jxc::format("Duplicate field {}::{}", annotation, key), parser.value());
            }

            // advance to field value
            if (!parser.next())
            {
                throw parse_error("Unexpected end of stream");
            }

            field->parse_field(parser, key, result);

            // NB. extra_field does not have a valid index, so this will only apply to normal defined fields
            if (field->index < assigned_fields.size())
            {
                assigned_fields[field->index] = true;
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
                    throw parse_error(jxc::format("Type {} is missing field {}", annotation, field.name), parser.value());
                }
            }
        }

        return result;
    }
};


JXC_BEGIN_NAMESPACE(detail)

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

JXC_END_NAMESPACE(detail)


template<typename T, typename FieldType, typename... TArgs>
FieldMetadata<T> def_field(const char* name, FieldType T::* field_ptr, TArgs&&... args)
{
    FieldMetadata<T> result(name);

    // update metadata based on argument types
    (detail::update_field<T>(result, std::forward<TArgs>(args)), ...);

    // set up default callbacks if we didn't get one as an argument
    if (!result.serialize_field)
    {
        result.serialize_field = detail::make_default_serialize_func<T>(field_ptr);
    }

    if (!result.parse_field)
    {
        result.parse_field = detail::make_default_parse_func<T>(field_ptr);
    }

    // get the field type name for use in error messages
    result.field_type_name = detail::get_type_name<FieldType>();

    return result;
}


// Like a field, but not real. Requires a defined getter and setter.
template<typename T, typename... TArgs>
FieldMetadata<T> def_property(const char* name, TArgs&&... args)
{
    FieldMetadata<T> result(name);

    // update metadata based on argument types
    (detail::update_field<T>(result, std::forward<TArgs>(args)), ...);

    JXC_ASSERTF(result.serialize_field || result.parse_field,
        "def_property({}) requires a parse function, a serialize function, or both",
        detail::debug_string_repr(name));

    return result;
}


template<typename T, typename... TArgs>
FieldMetadata<T> def_extra(TArgs&&... args)
{
    FieldMetadata<T> result("");

    // update metadata based on argument types
    (detail::update_field<T>(result, std::forward<TArgs>(args)), ...);

    JXC_ASSERTF(result.serialize_field || result.parse_field,
        "def_extra() requires a parse function, a serialize function, or both");

    return result;
}


template<typename T>
StructConverterMetadata<T> def_struct(const char* name, std::initializer_list<FieldMetadata<T>> fields, FieldFlags flags = FieldFlags::None)
{
    return StructConverterMetadata<T>{ std::string_view(name), fields, flags };
}


template<typename T>
StructConverterMetadata<T> def_struct(const TokenList& annotation, std::initializer_list<FieldMetadata<T>> fields, FieldFlags flags = FieldFlags::None)
{
    return StructConverterMetadata<T>{ annotation, fields, flags };
}


template<typename T>
StructConverterMetadata<T> def_struct(TokenList&& annotation, std::initializer_list<FieldMetadata<T>> fields, FieldFlags flags = FieldFlags::None)
{
    return StructConverterMetadata<T>{ std::forward<TokenList>(annotation), fields, flags };
}


#define JXC_DEFINE_AUTO_STRUCT_CONVERTER(CPP_TYPE, ANNOTATION, ...) \
    template<> \
    struct jxc::Converter<CPP_TYPE> { \
        using value_type = CPP_TYPE; \
        static const ::jxc::TokenList& get_annotation() { \
            static const ::jxc::TokenList anno = ::jxc::TokenList::parse_annotation_checked(ANNOTATION); \
            return anno; \
        } \
        static const ::jxc::StructConverterMetadata<value_type>& fields() { \
            static const ::jxc::StructConverterMetadata<value_type> data = StructConverterMetadata<value_type>(get_annotation(), { __VA_ARGS__ }); \
            return data; \
        } \
        static void serialize(::jxc::Serializer& doc, const value_type& value) { \
            fields().serialize(doc, value); \
        } \
        static value_type parse(::jxc::conv::Parser& parser, ::jxc::TokenView generic_anno) { \
            return fields().parse(parser, generic_anno); \
        } \
    }


#define JXC_FIELD(FIELD_NAME, ...) ::jxc::def_field<value_type>(#FIELD_NAME, &value_type::FIELD_NAME, ##__VA_ARGS__)

#define JXC_PROPERTY(FIELD_NAME, ...) ::jxc::def_property<value_type>(#FIELD_NAME, ##__VA_ARGS__)

#define JXC_EXTRA(...) ::jxc::def_extra<value_type>(__VA_ARGS__)


JXC_END_NAMESPACE(jxc)
