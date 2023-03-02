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
#include <memory>
#include <bitset>
#include <filesystem>


JXC_BEGIN_NAMESPACE(jxc)

JXC_BEGIN_NAMESPACE(conv)

template<typename T>
void serialize_array(Serializer& doc, const T& value, const TokenList& annotation)
{
    using VT = typename T::value_type;
    annotation.serialize(doc);
    doc.array_begin();
    for (const auto& item : value)
    {
        Converter<VT>::serialize(doc, item);
    }
    doc.array_end();
}

template<typename VT, typename Lambda>
void parse_array(conv::Parser& parser, const char* array_type, TokenView value_anno, Lambda&& value_callback)
{
    parser.require(ElementType::BeginArray);
    while (parser.next())
    {
        if (parser.value().type == jxc::ElementType::EndArray)
        {
            return;
        }
        value_callback(std::forward<VT>(parser.parse_value<VT>(value_anno)));
    }
    throw parse_error(jxc::format("Unexpected end of stream while parsing {}", array_type));
}

template<typename T, typename KT, typename VT>
void serialize_map(Serializer& doc, const T& value, const TokenList& annotation)
{
    annotation.serialize(doc);
    doc.object_begin();
    for (const auto& pair : value)
    {
        // key
        if constexpr (std::is_convertible_v<KT, std::string_view>)
        {
            doc.identifier_or_string(pair.first);
        }
        else
        {
            Converter<KT>::serialize(doc, pair.first);
        }
        doc.object_sep();

        // value
        Converter<VT>::serialize(doc, pair.second);
    }
    doc.object_end();
}

template<typename KT, typename VT, typename Lambda>
void parse_map(conv::Parser& parser, const char* map_type, TokenView key_anno, TokenView value_anno, Lambda&& pair_callback)
{
    parser.require(ElementType::BeginObject);
    while (parser.next())
    {
        if (parser.value().type == ElementType::EndObject)
        {
            return;
        }

        parser.require(ElementType::ObjectKey);

        KT key = parser.parse_value<KT>(key_anno);
        if (!parser.next())
        {
            throw parse_error(jxc::format("Unexpected end of stream while parsing {}", map_type));
        }

        pair_callback(std::forward<KT>(key), std::forward<VT>(parser.parse_value<VT>(value_anno)));
    }

    throw parse_error(jxc::format("Unexpected end of stream while parsing {}", map_type));
}

template<typename T, typename KT, typename VT>
T parse_map(conv::Parser& parser, const char* map_type, TokenView key_anno, TokenView value_anno)
{
    T result;

    parser.require(ElementType::BeginObject);
    while (parser.next())
    {
        if (parser.value().type == ElementType::EndObject)
        {
            return result;
        }

        parser.require(ElementType::ObjectKey);

        KT key = parser.parse_value<KT>(key_anno);
        if (!parser.next())
        {
            throw parse_error(jxc::format("Unexpected end of stream while parsing {}", map_type));
        }

        result.insert(std::make_pair<KT, VT>(
            std::forward<KT>(key),
            std::forward<VT>(parser.parse_value<VT>(value_anno))));
    }

    throw parse_error(jxc::format("Unexpected end of stream while parsing {}", map_type));
}

JXC_END_NAMESPACE(conv)


template<typename T, size_t N>
struct Converter<std::array<T, N>>
{
    using value_type = std::array<T, N>;

    static const TokenList& get_annotation()
    {
        static const TokenList anno = TokenList::parse_annotation_checked(jxc::format("std.array<{}, {}>", Converter<T>::get_annotation(), N));
        return anno;
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        value_type result;

        // annotation of the array's inner value type
        TokenList inner_generic_anno;

        parser.require(ElementType::BeginArray);
        if (TokenView array_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(array_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "array");
            anno_parser.require(TokenType::AngleBracketOpen);

            inner_generic_anno = TokenList(anno_parser.skip_over_generic_value());
            if (inner_generic_anno.size() == 0)
            {
                throw parse_error("Empty array value type", parser.value());
            }

            anno_parser.require_then_advance(TokenType::Comma);
            anno_parser.require(TokenType::Number);

            // parse the number and compare against the actual array size
            size_t anno_array_sz = 0;
            ErrorInfo err;
            if (!util::parse_number_simple(anno_parser.current(), anno_array_sz, err))
            {
                throw parse_error(jxc::format("Failed parsing number {} in annotation {}",
                    detail::debug_string_repr(anno_parser.current().value.as_view()), detail::debug_string_repr(array_anno.source())),
                    err);
            }

            if (anno_array_sz != N)
            {
                throw parse_error(jxc::format("Array size mismatch: can't parse array of size {} into type {}", anno_array_sz, get_annotation()),
                    parser.value());
            }

            if (!anno_parser.advance())
            {
                throw parse_error("Unexpected end of stream parsing annotation");
            }

            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

        TokenView inner_generic_anno_view = TokenView(inner_generic_anno);

        size_t array_len = 0;
        while (parser.next())
        {
            if (parser.value().type == ElementType::EndArray)
            {
                if (array_len != N)
                {
                    throw parse_error(jxc::format("Expected array of length {}, got {}", N, array_len), parser.value());
                }
                break;
            }
            else if (array_len >= N)
            {
                throw parse_error(jxc::format("Expected array of length {}, got at least {}", N, array_len + 1), parser.value());
            }

            JXC_DEBUG_ASSERT(array_len < N);
            result[array_len] = parser.parse_value<T>(inner_generic_anno_view);
            ++array_len;
        }

        return result;
    }
};


template<typename T>
    requires (!traits::Character<T> && !std::same_as<T, uint8_t>)
struct Converter<std::vector<T>>
{
    using value_type = std::vector<T>;

    static const TokenList& get_annotation()
    {
        static const TokenList anno = TokenList::parse_annotation_checked(jxc::format("std.vector<{}>", Converter<T>::get_annotation()));
        return anno;
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        value_type result;

        TokenList value_anno;
        if (TokenView vector_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(vector_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "vector");
            anno_parser.require(TokenType::AngleBracketOpen);
            value_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

        conv::parse_array<T>(parser, "std::vector", TokenView(value_anno),
            [&result](T&& item)
            {
                result.push_back(std::forward<T>(item));
            });

        return result;
    }
};


template<typename KT, typename VT>
struct Converter<std::map<KT, VT>>
{
    using value_type = std::map<KT, VT>;

    static const TokenList& get_annotation()
    {
        static const TokenList& anno = TokenList::parse_annotation_checked(jxc::format("std.map<{}, {}>", Converter<KT>::get_annotation(), Converter<VT>::get_annotation()));
        return anno;
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        conv::serialize_map<value_type, KT, VT>(doc, value, get_annotation());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        TokenList key_anno;
        TokenList value_anno;
        if (TokenView map_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(map_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "map");
            anno_parser.require(TokenType::AngleBracketOpen);
            key_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require(TokenType::Comma);
            value_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

        return conv::parse_map<value_type, KT, VT>(parser, "std::map", TokenView(key_anno), TokenView(value_anno));
    }
};


template<typename KT, typename VT>
struct Converter<std::unordered_map<KT, VT>>
{
    using value_type = std::unordered_map<KT, VT>;

    static const TokenList& get_annotation()
    {
        static const TokenList anno = TokenList::parse_annotation_checked(jxc::format("std.unordered_map<{}, {}>", Converter<KT>::get_annotation(), Converter<VT>::get_annotation()));
        return anno;
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        conv::serialize_map<value_type, KT, VT>(doc, value, get_annotation());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        TokenList key_anno;
        TokenList value_anno;
        if (TokenView map_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(map_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "unordered_map");
            anno_parser.require(TokenType::AngleBracketOpen);
            key_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require(TokenType::Comma);
            value_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

        return conv::parse_map<value_type, KT, VT>(parser, "std::unordered_map", TokenView(key_anno), TokenView(value_anno));
    }
};


template<typename T>
struct Converter<std::optional<T>>
{
    using value_type = std::optional<T>;

    static TokenList get_annotation()
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
            doc.value_null();
        }
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        if (TokenView anno = parser.get_value_annotation(generic_anno))
        {
            if (anno != get_annotation())
            {
                throw parse_error(jxc::format("Unexpected annotation {} for type {}", get_annotation(), detail::debug_string_repr(anno.source())),
                    parser.value());
            }
        }

        if (parser.value().token.type == TokenType::Null)
        {
            return std::nullopt;
        }
        return parser.parse_value<T>();
    }
};


template<typename T>
struct Converter<std::unique_ptr<T>>
{
    using value_type = std::unique_ptr<T>;

    static TokenList get_annotation()
    {
        return Converter<T>::get_annotation();
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        if (value.get() != nullptr)
        {
            Converter<T>::serialize(doc, *value.get());
        }
        else
        {
            doc.value_null();
        }
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        if (TokenView anno = parser.get_value_annotation(generic_anno))
        {
            parser.require_annotation(anno, get_annotation());
        }

        if (parser.value().token.type == TokenType::Null)
        {
            return nullptr;
        }

        return std::make_unique<T>(std::move(parser.parse_value<T>()));
    }
};


template<typename T>
struct Converter<std::shared_ptr<T>>
{
    using value_type = std::shared_ptr<T>;

    static TokenList get_annotation()
    {
        return Converter<T>::get_annotation();
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        if (value.get() != nullptr)
        {
            Converter<T>::serialize(doc, *value.get());
        }
        else
        {
            doc.value_null();
        }
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        if (TokenView anno = parser.get_value_annotation(generic_anno))
        {
            parser.require_annotation(anno, get_annotation());
        }

        if (parser.value().token.type == TokenType::Null)
        {
            return nullptr;
        }

        return std::make_shared<T>(std::move(parser.parse_value<T>()));
    }
};


template<typename T>
struct Converter<std::set<T>>
{
    using value_type = std::set<T>;

    static const TokenList& get_annotation()
    {
        static const TokenList anno = TokenList::parse_annotation_checked(jxc::format("std.set<{}>", Converter<T>::get_annotation()));
        return anno;
    }

    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        value_type result;

        TokenList value_anno;
        if (TokenView set_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(set_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "set");
            anno_parser.require(TokenType::AngleBracketOpen);
            value_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

        conv::parse_array<T>(parser, "std::set", TokenView(value_anno),
            [&result](T&& item)
            {
                result.insert(std::forward<T>(item));
            });
        
        return result;
    }
};


template<typename T>
struct Converter<std::unordered_set<T>>
{
    using value_type = std::unordered_set<T>;

    static const TokenList& get_annotation()
    {
        static const TokenList anno = TokenList::parse_annotation_checked(jxc::format("std.unordered_set<{}>", Converter<T>::get_annotation()));
        return anno;
    }

    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        conv::serialize_array(doc, value, get_annotation());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        value_type result;

        TokenList value_anno;
        if (TokenView set_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(set_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "unordered_set");
            anno_parser.require(TokenType::AngleBracketOpen);
            value_anno = TokenList(anno_parser.skip_over_generic_value());
            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

        conv::parse_array<T>(parser, "std::unordered_set", TokenView(value_anno),
            [&result](T&& item)
            {
                result.insert(std::forward<T>(item));
            });
        
        return result;
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

    static const TokenList& get_annotation()
    {
        static const TokenList anno = ([]() -> TokenList
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
                ss << Converter<arg_t>::get_annotation().source();
            });
            ss << ">";
            return TokenList::parse_annotation_checked(ss.str());
        })();
        return anno;
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

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        value_type result;

        parser.require(ElementType::BeginArray);

        detail::ArrayBuffer<TokenList, 8> value_annotations;

        if (TokenView tuple_anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(tuple_anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "tuple");
            anno_parser.require(TokenType::AngleBracketOpen);

            for (size_t i = 0; i < num_items; i++)
            {
                if (i > 0)
                {
                    anno_parser.require(TokenType::Comma);
                }
                value_annotations.push(TokenList(anno_parser.skip_over_generic_value()));
            }

            anno_parser.require_then_advance(TokenType::AngleBracketClose);
            anno_parser.done_required();
        }

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
                    TokenView value_anno = (idx < value_annotations.size()) ? TokenView(value_annotations[idx]) : TokenView();
                    out_arg = parser.parse_value<arg_t>(value_anno);
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

    static const TokenList& get_annotation()
    {
        static const TokenList anno = TokenList::parse_annotation_checked("std.filesystem.path");
        return anno;
    }

    static void serialize(jxc::Serializer& doc, const value_type& value)
    {
        doc.value_string_raw(value.string());
    }

    static value_type parse(conv::Parser& parser, TokenView generic_anno)
    {
        parser.require(ElementType::String);

        if (TokenView anno = parser.get_value_annotation(generic_anno))
        {
            AnnotationParser anno_parser = parser.make_annotation_parser(anno);
            if (anno_parser.equals(TokenType::Identifier, "std"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            if (anno_parser.equals(TokenType::Identifier, "filesystem"))
            {
                anno_parser.advance_required();
                anno_parser.require_then_advance(TokenType::Period);
            }
            anno_parser.require_then_advance(TokenType::Identifier, "path");
            anno_parser.done_required();
        }

        ErrorInfo err;
        std::string result;
        if (!util::parse_string_token(parser.value().token, result, err))
        {
            throw parse_error("Failed parsing path string", err);
        }
        return value_type(result);
    }
};

JXC_END_NAMESPACE(jxc)
