#pragma once
#include "jxc_cpp/jxc_converter.h"
#include "jxc_cpp/jxc_map.h"
#include <unordered_set>


JXC_BEGIN_NAMESPACE(jxc)


enum class EnumConverterStyle : uint8_t
{
    Default = 0,
    ValueAsInteger,
    NameAsString,
    NameAsExpression,
};


template<typename T, EnumConverterStyle Style = EnumConverterStyle::Default>
class EnumConverterMetadata
{
    static_assert(std::is_enum_v<T>, "Expected enum type");
    static_assert(std::is_integral_v<std::underlying_type_t<T>>, "Expected enum base type to be an integer");

public:
    using value_type = T;
    using base_type = std::underlying_type_t<value_type>;
    static constexpr bool base_type_unsigned = std::is_unsigned_v<base_type>;
    static constexpr EnumConverterStyle serialize_style = Style;

private:
    OwnedTokenSpan annotation;
    StringMap<value_type> name_map;
    ankerl::unordered_dense::map<value_type, std::string> value_map;

public:
    EnumConverterMetadata(const OwnedTokenSpan& annotation, std::initializer_list<std::pair<std::string, value_type>> values)
        : annotation(annotation)
    {
        for (const auto& pair : values)
        {
            JXC_ASSERTF(!name_map.contains(pair.first), "Duplicate enum value name {}", pair.first);
            name_map.insert(pair);
            value_map.insert({ pair.second, pair.first });
        }
    }

    void serialize(Serializer& doc, const value_type& value) const
    {
        annotation.serialize(doc);

        if constexpr (serialize_style == EnumConverterStyle::ValueAsInteger)
        {
            if constexpr (base_type_unsigned)
            {
                doc.value_uint(static_cast<base_type>(value));
            }
            else
            {
                doc.value_int(static_cast<base_type>(value));
            }
        }
        else if constexpr (serialize_style == EnumConverterStyle::NameAsExpression)
        {
            auto iter = value_map.find(value);
            if (iter != value_map.end())
            {
                doc.expression_begin()
                    .identifier_or_string(iter->second)
                    .expression_end();
            }
            else
            {
                if constexpr (base_type_unsigned)
                {
                    doc.expression_begin().value_uint(static_cast<base_type>(value)).expression_end();
                }
                else
                {
                    doc.expression_begin().value_int(static_cast<base_type>(value)).expression_end();
                }
            }
        }
        else
        {
            // Default and NameAsString styles
            auto iter = value_map.find(value);
            if (iter != value_map.end())
            {
                doc.value_string(iter->second);
            }
            else
            {
                // fallback on int - we don't have the correct value
                if constexpr (base_type_unsigned)
                {
                    doc.value_uint(static_cast<base_type>(value));
                }
                else
                {
                    doc.value_int(static_cast<base_type>(value));
                }
            }
        }
    }

    value_type parse(conv::Parser& parser, TokenSpan generic_anno) const
    {
        if (annotation)
        {
            TokenSpan enum_anno = parser.get_value_annotation(generic_anno);
            if (enum_anno && enum_anno != annotation)
            {
                throw parse_error(jxc::format("Invalid annotation {} for enum type {}", enum_anno.to_repr(), annotation), parser.value());
            }
        }

        auto identifier_to_value = [this](const Token& tok) -> value_type
        {
            JXC_DEBUG_ASSERT(tok.type == TokenType::Identifier);
            std::string_view val_str = tok.value.as_view();
            auto iter = name_map.find(val_str);
            if (iter == name_map.end())
            {
                throw parse_error(jxc::format("Enum {} has no value {}", annotation, detail::debug_string_repr(val_str)), tok);
            }
            return iter->second;
        };

        auto string_to_value = [this](const Token& tok) -> value_type
        {
            JXC_DEBUG_ASSERT(tok.type == TokenType::String);
            ErrorInfo err;
            std::string val_str;
            if (!util::parse_string_token(tok, val_str, err))
            {
                throw parse_error("Failed to parse enum value string", err);
            }
            
            auto iter = name_map.find(val_str);
            if (iter == name_map.end())
            {
                throw parse_error(jxc::format("Enum {} has no value {}", annotation, detail::debug_string_repr(val_str)), tok);
            }
            return iter->second;
        };

        auto number_to_value = [this](const Token& tok) -> value_type
        {
            JXC_DEBUG_ASSERT(tok.type == TokenType::Number);
            ErrorInfo err;
            std::string suffix;
            base_type parsed_value = static_cast<base_type>(0);
            if (!util::parse_number_simple<base_type>(tok, parsed_value, err, &suffix))
            {
                throw parse_error(jxc::format("Failed to parse enum value number"), err);
            }

            if (suffix.size() > 0)
            {
                throw parse_error(jxc::format("Unexpected numeric suffix {} on enum value", detail::debug_string_repr(suffix)), tok);
            }

            const value_type result = static_cast<value_type>(parsed_value);
            if (!value_map.contains(result))
            {
                throw parse_error(jxc::format("Enum {} has no value {}", annotation, parsed_value), tok);
            }
            return result;
        };

        value_type result = static_cast<value_type>(0);

        switch (parser.value().type)
        {
        case ElementType::BeginExpression:
            if (!parser.next())
            {
                throw parse_error("Unexpected end of stream parsing enum");
            }

            switch (parser.value().token.type)
            {
            case TokenType::Identifier:
                result = identifier_to_value(parser.value().token);
                break;
            case TokenType::String:
                result = string_to_value(parser.value().token);
                break;
            case TokenType::Number:
                result = number_to_value(parser.value().token);
                break;
            default:
                throw parse_error(jxc::format("Expected Identifier, String, or Number, got {}", token_type_to_string(parser.value().token.type)), parser.value());
            }

            if (!parser.next())
            {
                throw parse_error("Unexpected end of stream parsing enum");
            }
            parser.require(ElementType::EndExpression);
            break;
        
        case ElementType::Number:
            result = number_to_value(parser.value().token);
            break;

        case ElementType::String:
            result = string_to_value(parser.value().token);
            break;

        default:
            throw parse_error(jxc::format("Expected Expression, Number, or String, got {}", element_type_to_string(parser.value().type)), parser.value());
        }

        return result;
    }
};


template<typename T>
inline std::pair<std::string, T> def_enum_value(const char* name, T value)
{
    static_assert(std::is_enum_v<T>, "def_enum_value requires an enum type");
    return std::make_pair(std::string(name), value);
}


template<typename T, typename... TArgs>
EnumConverterMetadata<T> def_enum(const char* name, TArgs&&... args)
{
    return EnumConverterMetadata<T>{ name, { std::forward<TArgs>(args)... } };
}


#define JXC_DEFINE_AUTO_ENUM_CONVERTER(ENUM_TYPE, ANNOTATION, ENUM_CONVERTER_STYLE, ...) \
    template<> \
    struct jxc::Converter<ENUM_TYPE> { \
        static_assert(std::is_enum_v<ENUM_TYPE>, "Expected type to be enum: " #ENUM_TYPE); \
        using value_type = ENUM_TYPE; \
        static const ::jxc::OwnedTokenSpan& get_annotation() { \
            static const ::jxc::OwnedTokenSpan anno = ::jxc::OwnedTokenSpan::parse_annotation_checked(ANNOTATION); \
            return anno; \
        } \
        static const ::jxc::EnumConverterMetadata<value_type, ENUM_CONVERTER_STYLE>& values() { \
            static const ::jxc::EnumConverterMetadata<value_type, ENUM_CONVERTER_STYLE> data = { get_annotation(), { __VA_ARGS__ } }; \
            return data; \
        } \
        static void serialize(::jxc::Serializer& doc, const value_type& value) { \
            values().serialize(doc, value); \
        } \
        static value_type parse(::jxc::conv::Parser& parser, ::jxc::TokenSpan generic_anno) { \
            return values().parse(parser, generic_anno); \
        } \
    }


#define JXC_ENUM_VALUE(NAME, ...) ::jxc::def_enum_value<value_type>(#NAME, value_type::NAME)


JXC_END_NAMESPACE(jxc)
