#pragma once
#include "jxc_cpp/jxc_converter.h"
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <set>
#include <unordered_set>
#include <tuple>
#include <variant>
#include <bitset>
#include <filesystem>


JXC_BEGIN_NAMESPACE(jxc)

JXC_BEGIN_NAMESPACE(detail)

struct AnnotationParser
{
    TokenSpan anno;
    size_t idx = 0;
    int64_t paren_depth = 0;
    int64_t angle_bracket_depth = 0;

    AnnotationParser(TokenSpan anno)
        : anno(anno)
    {
        if (anno.size() > 0 && (
            anno[0].type == TokenType::AngleBracketOpen
            || anno[0].type == TokenType::AngleBracketClose
            || anno[0].type == TokenType::ParenOpen
            || anno[0].type == TokenType::ParenClose))
        {
            throw parse_error(jxc::format("Annotations may not start with token {}", token_type_to_string(anno[0].type)));
        }
    }

    bool advance()
    {
        ++idx;
        if (idx < anno.size())
        {
            switch (anno[idx].type)
            {
            case TokenType::AngleBracketOpen:
                ++angle_bracket_depth;
                break;
            case TokenType::AngleBracketClose:
                --angle_bracket_depth;
                if (angle_bracket_depth < 0)
                {
                    throw parse_error("Unmatched close angle bracket");
                }
                break;
            case TokenType::ParenOpen:
                if (angle_bracket_depth <= 0)
                {
                    throw parse_error("parens can only appear in annotations inside angle brackets");
                }
                ++paren_depth;
                break;
            case TokenType::ParenClose:
                --paren_depth;
                if (paren_depth < 0)
                {
                    throw parse_error("Unmatched close paren");
                }
                break;
            default:
                break;
            }
        }
        else
        {
            return false;
        }
        return true;
    }

    void advance_required()
    {
        if (!advance())
        {
            throw parse_error(jxc::format("Unexpected end of stream while parsing annotation {}", detail::debug_string_repr(anno.source().as_view())));
        }
    }

    bool done() const
    {
        return idx >= anno.size();
    }

    void done_required()
    {
        if (!done())
        {
            throw parse_error(jxc::format("Expected end of stream, got {}", token_type_to_string(anno[idx].type)));
        }
    }

    const Token& current() const
    {
        JXC_ASSERT(idx < anno.size());
        return anno[idx];
    }

    void require(TokenType tok_type, std::string_view tok_value = std::string_view{}) const
    {
        if (done())
        {
            throw parse_error(jxc::format("Unexpected end of stream while parsing annotation {}", detail::debug_string_repr(anno.source().as_view())));
        }
        else if (anno[idx].type != tok_type)
        {
            throw parse_error(jxc::format("Expected token type {}, got {}", token_type_to_string(tok_type), token_type_to_string(anno[idx].type)));
        }
        else if (tok_value.size() > 0 && anno[idx].value.as_view() != tok_value)
        {
            throw parse_error(jxc::format("Expected token value {}, got {}", detail::debug_string_repr(tok_value), detail::debug_string_repr(anno[idx].value.as_view())));
        }
    }

    bool require_then_advance(TokenType tok_type, std::string_view tok_value = std::string_view{})
    {
        require(tok_type, tok_value);
        return advance();
    }

    void skip_over_generic_value()
    {
        const int64_t orig_angle = angle_bracket_depth;
        const int64_t orig_paren = paren_depth;
        while (advance())
        {
            if (orig_angle > 0 && angle_bracket_depth == orig_angle - 1 && anno[idx].type == TokenType::AngleBracketClose)
            {
                // we started inside angle brackets, those angle brackets just closed
                return;
            }
            else if (orig_angle > 0 && orig_angle == angle_bracket_depth && orig_paren == paren_depth && anno[idx].type == TokenType::Comma)
            {
                // we started inside angle brackets, our current angle and paren levels have not changed, and we hit a comma, so we're done
                return;
            }
        }
    }
};

JXC_END_NAMESPACE(detail)


JXC_BEGIN_NAMESPACE(conv)

template<typename T>
void serialize_array(Serializer& doc, const T& value, const std::string& annotation)
{
    using VT = typename T::value_type;
    doc.annotation(annotation);
    doc.array_begin();
    for (const auto& item : value)
    {
        Converter<VT>::serialize(doc, item);
    }
    doc.array_end();
}

template<typename T, typename AddFunc>
T parse_array(JumpParser& parser, AddFunc&& add_func)
{
    using VT = typename T::value_type;
    T result;
    require(parser.value().type, ElementType::BeginArray);
    while (parser.next())
    {
        if (parser.value().type == jxc::ElementType::EndArray)
        {
            return result;
        }
        add_func(result, Converter<VT>::parse(parser));
    }
    throw parse_error("Unexpected end of stream while parsing array");
}

template<typename T, typename KT, typename VT>
void serialize_map(Serializer& doc, const T& value, const std::string& annotation)
{
    doc.annotation(annotation);
    doc.object_begin();
    for (const auto& pair : value)
    {
        Converter<KT>::serialize(doc, pair.first);
        doc.object_sep();
        Converter<VT>::serialize(doc, pair.second);
    }
    doc.object_end();
}

template<typename T, typename KT, typename VT>
T parse_map(JumpParser& parser, const std::string& annotation)
{
    T result;

    conv::require(parser.value().type, ElementType::BeginObject);
    while (parser.next())
    {
        if (parser.value().type == ElementType::EndObject)
        {
            return result;
        }

        conv::require(parser.value().type, ElementType::ObjectKey);

        KT key = Converter<KT>::parse(parser);
        if (!parser.next())
        {
            throw parse_error(jxc::format("Unexpected end of stream while parsing type {}", annotation));
        }

        result.insert(std::make_pair<KT, VT>(key, Converter<VT>::parse(parser)));
    }

    throw parse_error(jxc::format("Unexpected end of stream while parsing type {}", annotation));
}

JXC_END_NAMESPACE(conv)


template<typename T, size_t N>
struct Converter<std::array<T, N>>
{
    using value_type = std::array<T, N>;

    static std::string get_annotation()
    {
        return jxc::format("std.array<{}, {}>", Converter<T>::get_annotation(), N);
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }

    static value_type parse(JumpParser& parser)
    {
        value_type result;

        conv::require(parser.value().type, ElementType::BeginArray);
        if (parser.value().annotation.size() > 0)
        {
            detail::AnnotationParser anno{ parser.value().annotation };
            anno.require_then_advance(TokenType::Identifier, "std");
            anno.require_then_advance(TokenType::Period);
            anno.require_then_advance(TokenType::Identifier, "array");
            anno.require_then_advance(TokenType::AngleBracketOpen);

            anno.skip_over_generic_value();

            anno.require_then_advance(TokenType::Comma);
            anno.require(TokenType::Number);

            // parse the number and compare against the actual array size
            size_t anno_array_sz = 0;
            std::string err;
            if (!util::parse_number_simple(anno.current().value.as_view(), anno_array_sz, &err))
            {
                throw parse_error(jxc::format("Failed parsing number {} in annotation {}",
                    detail::debug_string_repr(anno.current().value.as_view()), detail::debug_string_repr(parser.value().annotation.source())));
            }

            if (anno_array_sz != N)
            {
                throw parse_error(jxc::format("Array size mismatch: can't parse array of size {} into type {}", anno_array_sz, get_annotation()));
            }

            if (!anno.advance())
            {
                throw parse_error("Unexpected end of stream parsing annotation");
            }

            anno.require_then_advance(TokenType::AngleBracketClose);
            anno.done_required();
        }

        size_t array_len = 0;
        while (parser.next())
        {
            if (parser.value().type == ElementType::EndArray)
            {
                if (array_len != N)
                {
                    throw parse_error(jxc::format("Expected array of length {}, got {}", N, array_len));
                }
                break;
            }
            else if (array_len >= N)
            {
                throw parse_error(jxc::format("Expected array of length {}, got at least {}", N, array_len + 1));
            }

            JXC_DEBUG_ASSERT(array_len < N);
            result[array_len] = Converter<T>::parse(parser);
            ++array_len;
        }

        return result;
    }
};


template<typename T>
    requires (!std::same_as<T, uint8_t>)
struct Converter<std::vector<T>>
{
    using value_type = std::vector<T>;
    static std::string get_annotation() { return jxc::format("std.vector<{}>", Converter<T>::get_annotation()); }
    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }
    static value_type parse(jxc::JumpParser& parser)
    {
        return conv::parse_array<value_type>(parser, [](std::vector<T>& container, T&& item)
        {
            container.push_back(std::forward<T>(item));
        });
    }
};


template<typename KT, typename VT>
struct Converter<std::map<KT, VT>>
{
    using value_type = std::map<KT, VT>;
    static std::string get_annotation() { return jxc::format("std.map<{}, {}>", Converter<KT>::get_annotation(), Converter<VT>::get_annotation()); }
    static void serialize(Serializer& doc, const value_type& value) { conv::serialize_map<value_type, KT, VT>(doc, value, get_annotation()); }
    static value_type parse(JumpParser& parser) { return conv::parse_map<value_type, KT, VT>(parser); }
};


template<typename KT, typename VT>
struct Converter<std::unordered_map<KT, VT>>
{
    using value_type = std::unordered_map<KT, VT>;
    static std::string get_annotation() { return jxc::format("std.unordered_map<{}, {}>", Converter<KT>::get_annotation(), Converter<VT>::get_annotation()); }
    static void serialize(Serializer& doc, const value_type& value) { conv::serialize_map<value_type, KT, VT>(doc, value, get_annotation()); }
    static value_type parse(JumpParser& parser) { return conv::parse_map<value_type, KT, VT>(parser); }
};


template<typename T>
struct Converter<std::optional<T>>
{
    using value_type = std::optional<T>;

    static std::string get_annotation()
    {
        return Converter<T>::get_annotation();
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        if (value.has_value())
        {
            Converter<T>::serialize(doc, value.value());
        }
        else
        {
            doc.annotation(get_annotation()).value_null();
        }
    }

    static value_type parse(JumpParser& parser)
    {
        if (parser.value().annotation.size() > 0 && !parser.value().annotation.equals_annotation_string_lexed(get_annotation()))
        {
            throw parse_error(jxc::format("Unexpected annotation {} for type {}",
                detail::debug_string_repr(get_annotation()),
                detail::debug_string_repr(parser.value().annotation.source())));
        }

        if (parser.value().token.type == TokenType::Null)
        {
            return std::nullopt;
        }
        return Converter<T>::parse(parser);
    }
};


template<typename T>
struct Converter<std::set<T>>
{
    using value_type = std::set<T>;
    static std::string get_annotation() { return jxc::format("std.set<{}>", Converter<T>::get_annotation()); }
    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }
    static value_type parse(jxc::JumpParser& parser)
    {
        return conv::parse_array<value_type>(parser, [](std::set<T>& container, T&& item)
        {
            container.insert(std::forward<T>(item));
        });
    }
};


template<typename T>
struct Converter<std::unordered_set<T>>
{
    using value_type = std::unordered_set<T>;
    static std::string get_annotation() { return jxc::format("std.unordered_set<{}>", Converter<T>::get_annotation()); }
    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }
    static value_type parse(jxc::JumpParser& parser)
    {
        return conv::parse_array<value_type>(parser, [](std::unordered_set<T>& container, T&& item)
        {
            container.insert(std::forward<T>(item));
        });
    }
};


JXC_BEGIN_NAMESPACE(conv)
    
template<typename TTuple, typename Lambda, size_t... IdxSeq>
constexpr void iter_tuple_impl(TTuple&& tup, Lambda&& func, std::index_sequence<IdxSeq...>)
{
    ( func(std::integral_constant<size_t, IdxSeq>{}, std::get<IdxSeq>(tup)),... );
}


template<typename... T, typename Lambda>
constexpr void iter_tuple(const std::tuple<T...>& tup, Lambda&& func)
{
    iter_tuple_impl(tup, std::forward<Lambda>(func), std::make_index_sequence<sizeof...(T)>{});
}

template<typename... T, typename Lambda>
constexpr void iter_tuple(std::tuple<T...>& tup, Lambda&& func)
{
    iter_tuple_impl(tup, std::forward<Lambda>(func), std::make_index_sequence<sizeof...(T)>{});
}


template<typename T>
using tuple_item_t = std::remove_cv_t<std::remove_reference_t<T>>;

JXC_END_NAMESPACE(conv)



template<typename... TArgs>
struct Converter<std::tuple<TArgs...>>
{
    using value_type = std::tuple<TArgs...>;
    static constexpr size_t num_items = sizeof...(TArgs);

    static std::string get_annotation()
    {
        std::ostringstream ss;
        ss << "std.tuple<";
        value_type tup;
        conv::iter_tuple(tup, [&](size_t i, auto& out_arg)
        {
            using arg_t = conv::tuple_item_t<decltype(out_arg)>;
            if (i > 0)
            {
                ss << ", ";
            }
            ss << Converter<arg_t>::get_annotation();
        });
        ss << ">";
        return ss.str();
    }

    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        doc.array_begin();
        conv::iter_tuple(value, [&](size_t i, const auto& arg)
        {
            using arg_t = conv::tuple_item_t<decltype(arg)>;
            Converter<arg_t>::serialize(doc, arg);
        });
        doc.array_end();
    }

    static value_type parse(jxc::JumpParser& parser)
    {
        value_type result;

        conv::require(parser.value().type, ElementType::BeginArray);

        size_t idx = 0;
        while (parser.next())
        {
            if (parser.value().type == ElementType::EndArray)
            {
                break;
            }

            conv::iter_tuple(result, [&](size_t i, auto& out_arg)
            {
                using arg_t = conv::tuple_item_t<decltype(out_arg)>;
                if (i == idx)
                {
                    out_arg = Converter<arg_t>::parse(parser);
                }
            });

            ++idx;
        }
        return result;
    }
};


template<>
struct Converter<std::filesystem::path>
{
    using value_type = std::filesystem::path;
    static std::string get_annotation() { return "std.filesystem.path"; }
    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        doc.annotation(get_annotation()).value_string_raw(value.string());
    }

    static value_type parse(jxc::JumpParser& parser)
    {
        conv::require(parser.value().type, ElementType::String);
        if (parser.value().annotation.size() > 0 && !parser.value().annotation.equals_annotation_string_lexed(get_annotation()))
        {
            throw parse_error(jxc::format("Unexpected annotation {} for type {}",
                detail::debug_string_repr(get_annotation()),
                detail::debug_string_repr(parser.value().annotation.source())));
        }
        ErrorInfo err;
        std::string result;
        if (!util::parse_string_token(parser.value().token, result, err))
        {
            throw parse_error(jxc::format("Failed parsing string: {}", err.to_string()));
        }
        return value_type(result);
    }
};

JXC_END_NAMESPACE(jxc)
