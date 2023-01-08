#pragma once
#include "ankerl/unordered_dense.h"
#include <string>
#include <string_view>


namespace jxc::python_util
{

struct StringHash
{
    using is_transparent = void; // enable heterogeneous overloads
    using is_avalanching = void; // mark class as high quality avalanching hash

    auto operator()(std::string_view str) const noexcept -> uint64_t
    {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }
};

struct StringEq
{
    using is_transparent = void; // enable heterogeneous overloads

    template<class T, class U>
    auto operator()(T&& lhs, U&& rhs) const -> decltype(std::forward<T>(lhs) == std::forward<U>(rhs))
    {
        return std::forward<T>(lhs) == std::forward<U>(rhs);
    }
};

template<typename T>
using StringMap = ankerl::unordered_dense::map<std::string, T, StringHash, StringEq>;

struct TokenSpanHash
{
    using is_transparent = void; // enable heterogeneous overloads
    using is_avalanching = void; // mark class as high quality avalanching hash

    auto operator()(const OwnedTokenSpan& anno) const noexcept -> uint64_t
    {
        return anno.hash();
    }

    auto operator()(TokenSpan anno) const noexcept -> uint64_t
    {
        return anno.hash();
    }

    auto operator()(std::string_view str) const noexcept -> uint64_t
    {
        return TokenSpan::hash_string_as_single_token(str);
    }
};

struct TokenSpanEq
{
    using is_transparent = void; // enable heterogeneous overloads

    template<class T, class U>
    auto operator()(T&& lhs, U&& rhs) const -> decltype(std::forward<T>(lhs) == std::forward<U>(rhs))
    {
        if constexpr (std::is_same_v<T, std::string_view> || std::is_convertible_v<T, std::string_view>)
        {
            return std::string_view(std::forward<T>(lhs)) == std::forward<U>(rhs);
        }
        else if constexpr (std::is_same_v<U, std::string_view> || std::is_convertible_v<U, std::string_view>)
        {
            return std::forward<T>(lhs) == std::string_view(std::forward<U>(rhs));
        }

        return std::forward<T>(lhs) == std::forward<U>(rhs);
    }
};

template<typename T>
using AnnotationMap = ankerl::unordered_dense::map<OwnedTokenSpan, T, TokenSpanHash, TokenSpanEq>;

} // namespace jxc::python_util
