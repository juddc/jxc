#pragma once
#include "ankerl/unordered_dense.h"
#include <string>
#include <string_view>


namespace jxc
{

namespace detail
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

} // namespace detail

template<typename T>
using StringMap = ankerl::unordered_dense::map<std::string, T, detail::StringHash, detail::StringEq>;

} // namespace jxc
