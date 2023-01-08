#pragma once
#if !defined(JXC_HAVE_CONCEPTS)
#if __cpp_concepts >= 201907L
#define JXC_HAVE_CONCEPTS 1
#else
#define JXC_HAVE_CONCEPTS 0
#endif
#endif

#include <stdint.h>
#include <string_view>
#include <vector>
#include <type_traits>
#include <limits>
#include <optional>
#include <cfloat>
#if JXC_HAVE_CONCEPTS
#include <concepts>
#endif

//NB. This header is expected to be standalone and should not include any jxc headers

// JXC_CONCEPT macro for falling back to `typename` if compling with C++17
#if !defined(JXC_CONCEPT)
#if JXC_HAVE_CONCEPTS
#define JXC_CONCEPT(CONCEPT) CONCEPT
#else
#define JXC_CONCEPT(CONCEPT) typename
#endif
#endif

namespace jxc
{

namespace tag
{
// Tag types for ease of constructing variants/unions
struct Invalid {};

// these tags are defined as enums so that static_cast works with them
enum class Null : uint8_t { Null = 0 };
enum class Bool : uint8_t { False = static_cast<uint8_t>(false), True = static_cast<uint8_t>(true) };
enum Integer : uint8_t {};
enum Float : uint8_t {};

struct String
{
    static constexpr const char* empty_string = "";
    constexpr inline const char* data() const { return empty_string; }
    constexpr inline size_t size() const { return 0; }
    constexpr operator std::string_view() const noexcept { return std::string_view{ empty_string, 0 }; }
};
struct Bytes
{
    constexpr inline const uint8_t* data() const { return nullptr; }
    constexpr inline size_t size() const { return 0; }
};
struct Array
{
    const void* data() const { return nullptr; }
    constexpr inline size_t size() const { return 0; }
};
struct Object
{
    constexpr inline size_t size() const { return 0; }
};
} // namespace tag


// useful constants for passing values into function or constructor overloads
static constexpr tag::Invalid default_invalid;
static constexpr tag::Null default_null = tag::Null::Null;
static constexpr tag::Bool default_bool = tag::Bool::False;
static constexpr tag::Integer default_integer = static_cast<tag::Integer>(0);
static constexpr tag::Float default_float = static_cast<tag::Float>(0);
static constexpr tag::String default_string;
static constexpr tag::Bytes default_bytes;
static constexpr tag::Array default_array;
static constexpr tag::Object default_object;

namespace traits
{

// NB. These numeric traits should be defined without using C++20 so they can be used as a fallback when compiling in C++17 mode

template<typename T>
static constexpr bool is_integer_type = std::is_same_v<T, tag::Integer>
    || (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_enum_v<T> && !std::is_same_v<T, char>);

template<typename T>
static constexpr bool is_float_type = std::is_same_v<T, tag::Float>
    || (std::is_floating_point_v<T> && !std::is_same_v<T, bool> && !std::is_enum_v<T> && !std::is_same_v<T, char>);

template<typename T>
static constexpr bool is_numeric_type = is_integer_type<T> || is_float_type<T>;


#if JXC_HAVE_CONCEPTS

template<typename T, typename ValueT>
concept HasDataAndSize = requires(T t)
{
    { t.data() } -> std::convertible_to<const ValueT*>;
    { t.size() } -> std::convertible_to<size_t>;
};

template<typename T>
concept IterableContainer = requires { typename T::iterator; }
&& requires(T t) { { t.begin() } -> std::same_as<typename T::iterator>; }
&& requires(T t) { { t.end() } -> std::same_as<typename T::iterator>; }
;

template<typename T>
concept Invalid = std::same_as<T, tag::Invalid>;

template<typename T>
concept Null = std::same_as<T, tag::Null> || std::is_null_pointer_v<T>;

template<typename T>
concept Bool = std::same_as<T, tag::Bool> || std::same_as<T, bool>;

template<typename T>
concept Integer = is_integer_type<T>;

template<typename T>
concept SignedInteger = Integer<T> && std::signed_integral<T>;

template<typename T>
concept UnsignedInteger = Integer<T> && std::unsigned_integral<T>;

template<typename T>
concept Float = is_float_type<T>;

template<typename T>
concept Number = is_numeric_type<T>;

template<typename T>
concept StringLiteral = !std::is_null_pointer_v<T> && std::same_as<T, const char*>;

// any string type that is generally passed by reference (eg. all strings except const char*)
template<typename T>
concept StringContainer = !std::is_null_pointer_v<T> && !std::is_array_v<T> && (
    HasDataAndSize<std::remove_cvref_t<T>, char> || (!std::is_pointer_v<std::remove_cvref_t<T>> && std::is_convertible_v<T, std::string_view>)
    );

template<typename T>
concept String = !std::is_null_pointer_v<T> && (std::same_as<T, const char*> || HasDataAndSize<T, char>) && std::is_convertible_v<T, std::string_view>;

template<typename T>
concept Bytes = HasDataAndSize<T, uint8_t>;

template<typename T, typename ValT>
concept Array = std::same_as<T, tag::Array> || (
    HasDataAndSize<T, ValT> && requires(T t, size_t idx)
{
    { t[idx] } -> std::same_as<ValT&>;
}
);

template<typename T, typename KeyT, typename ValT>
concept Object = std::same_as<T, tag::Object>
|| (requires
{
    // hashable key type
    { std::hash<KeyT>()() } -> std::convertible_to<uint64_t>;
}
&& requires(T t, const KeyT& key)
{
    { t.size() } -> std::same_as<size_t>;
    { t.contains(key) } -> std::same_as<bool>;
    { t[key] } -> std::same_as<ValT&>;
}
&& IterableContainer<T>
&& requires (T t, const KeyT& key, const ValT& val)
{
    { t.insert_or_assign(key, val) };
}
);

template<typename T>
concept ObjectKey = Null<T> || Bool<T> || Integer<T> || StringLiteral<T> || (
    std::is_reference_v<T> && (StringContainer<std::remove_cvref_t<T>> || Bytes<std::remove_cvref_t<T>>));

template<typename T>
concept ConstructibleFromIterator = requires { typename T::iterator; } && std::constructible_from<T, typename T::iterator, typename T::iterator>;

static_assert(ConstructibleFromIterator<std::vector<int>>,
    "std::vector can be constructed with a begin and end iterator");

template<typename T, typename ValT>
concept ConstructibleFromTypedIterator = requires
{
    typename T::iterator;
    typename T::iterator::value_type;
} && std::constructible_from<T, typename T::iterator, typename T::iterator>
    && std::same_as<ValT, typename T::iterator::value_type>;

static_assert(ConstructibleFromTypedIterator<std::vector<int>, int>,
    "std::vector<int> can be constructed with a begin and end iterator using value_type int");

// Owned container type with resizable buffer
template<typename T, typename ValueT>
concept Buffer = requires(T t, size_t sz)
{
    { t.resize(sz) };
    { t.data() } -> std::same_as<ValueT*>;
    { t.size() } -> std::same_as<size_t>;
};

// Owned string type with resizable buffer
template<typename T>
concept StringBuffer = Buffer<T, char>;

// Owned byte array type with resizable buffer
template<typename T>
concept ByteBuffer = Buffer<T, uint8_t>;

static_assert(StringBuffer<std::string>&& StringBuffer<std::vector<char>>,
    "std::string and std::vector<char> are valid StringBuffer types");

template<typename T>
concept OwnedByteArray = Bytes<T> && (ConstructibleFromTypedIterator<T, uint8_t> || std::constructible_from<T, const uint8_t*, size_t>);

template<typename T>
concept OwnedString = String<T> && (ConstructibleFromTypedIterator<T, char> || std::constructible_from<T, const char*, size_t>);

template<typename T>
concept HasToString = requires(T t)
{
    { t.to_string() } -> String;
};

template<typename T>
concept HasToRepr = requires(T t)
{
    { t.to_repr() } -> String;
};

#endif // JXC_HAVE_CONCEPTS

// convert any type that qualifies as traits::Bool to an actual bool value
template<typename ResultType = bool, JXC_CONCEPT(Bool) T>
constexpr inline ResultType cast_bool(T val)
{
#if !JXC_HAVE_CONCEPTS
    static_assert(std::is_same_v<T, tag::Bool> || std::is_same_v<T, bool> || is_integer_type<T>,
        "cast_bool requires a type that is convertible to bool");
#endif
    return static_cast<ResultType>(val);
}


enum class OutOfBoundsType : uint8_t
{
    NotANumber = 0,
    Success,
    Low,
    High,
};


template<typename T>
struct NumericCastResult
{
    T value = static_cast<T>(0);
    OutOfBoundsType type = OutOfBoundsType::NotANumber;

    constexpr NumericCastResult() {}
    constexpr NumericCastResult(T value) : value(value), type(OutOfBoundsType::Success) {}
    constexpr NumericCastResult(OutOfBoundsType type) : type(type) {}
    constexpr inline explicit operator bool() const { return type == OutOfBoundsType::Success; }
    constexpr inline std::optional<T> as_optional() const { return (type == OutOfBoundsType::Success) ? std::optional<T>(value) : std::nullopt; }
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

template<typename Dst, typename Src>
constexpr NumericCastResult<Dst> cast_integer(Src val)
{
    static_assert(is_integer_type<Src> && is_integer_type<Dst>, "cast_integer only supports integers");

#if defined(__cpp_lib_integer_comparison_functions)
    if (std::cmp_greater(val, std::numeric_limits<Dst>::max()))
    {
        return NumericCastResult<Dst>(OutOfBoundsType::High);
    }
    else if (std::cmp_less(val, std::numeric_limits<Dst>::min()))
    {
        return NumericCastResult<Dst>(OutOfBoundsType::Low);
    }
    else
    {
        return NumericCastResult<Dst>(static_cast<Dst>(val));
    }
#else
    // adapted from https://stackoverflow.com/a/61552951
    constexpr bool from_signed = std::is_signed_v<Src>;
    constexpr bool to_signed = std::is_signed_v<Dst>;
    constexpr bool both_signed = from_signed && to_signed;
    constexpr bool both_unsigned = !from_signed && !to_signed;

    //constexpr Src src_max = std::numeric_limits<Src>::max();
    //constexpr Src src_min = std::numeric_limits<Src>::min();
    constexpr Dst dst_max = std::numeric_limits<Dst>::max();
    constexpr Dst dst_min = std::numeric_limits<Dst>::min();

    if constexpr (both_unsigned)
    {
        using Widen = std::conditional_t<(sizeof(Src) > sizeof(Dst)), Src, Dst>;
        if (val > Widen(dst_max))
        {
            return NumericCastResult<Dst>(OutOfBoundsType::High);
        }
        else
        {
            return NumericCastResult<Dst>(static_cast<Dst>(val));
        }
    }
    else if constexpr (both_signed)
    {
        using Widen = std::conditional_t<(sizeof(Src) > sizeof(Dst)), Src, Dst>;
        if (val > Widen(dst_max))
        {
            return NumericCastResult<Dst>(OutOfBoundsType::High);
        }
        else if (val < Widen(dst_min))
        {
            return NumericCastResult<Dst>(OutOfBoundsType::Low);
        }
        else
        {
            return NumericCastResult<Dst>(static_cast<Dst>(val));
        }
    }
    else if constexpr (from_signed && !to_signed)
    {
        using Widen = std::make_unsigned_t<std::conditional_t<(sizeof(Src) > sizeof(Dst)), Src, Dst>>;
        if (val < 0)
        {
            return NumericCastResult<Dst>(OutOfBoundsType::Low);
        }
        else if (val > Widen(dst_max))
        {
            return NumericCastResult<Dst>(OutOfBoundsType::High);
        }
        else
        {
            return NumericCastResult<Dst>(static_cast<Dst>(val));
        }
    }
    else if constexpr (!from_signed && to_signed)
    {
        using Widen = std::make_unsigned_t<std::conditional_t<(sizeof(Src) > sizeof(Dst)), Src, Dst>>;
        if (val > Widen(dst_max))
        {
            return NumericCastResult<Dst>(OutOfBoundsType::High);
        }
        else
        {
            return NumericCastResult<Dst>(static_cast<Dst>(val));
        }
    }
#endif
}


#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


template<typename Dst, typename Src>
constexpr inline std::optional<Dst> cast_integer_safe(Src val)
{
    return cast_integer<Dst, Src>(val).as_optional();
}


namespace detail::int_cast_tests
{
constexpr uint64_t large_pos64 = 10000000000000000000ull;
static_assert(cast_integer_safe<uint8_t>(large_pos64) == std::nullopt);
static_assert(cast_integer_safe<uint16_t>(large_pos64) == std::nullopt);
static_assert(cast_integer_safe<uint32_t>(large_pos64) == std::nullopt);
static_assert(cast_integer_safe<uint64_t>(large_pos64) == 10000000000000000000ull);
static_assert(cast_integer_safe<int8_t>(large_pos64) == std::nullopt);
static_assert(cast_integer_safe<int16_t>(large_pos64) == std::nullopt);
static_assert(cast_integer_safe<int32_t>(large_pos64) == std::nullopt);
static_assert(cast_integer_safe<int64_t>(large_pos64) == std::nullopt);

constexpr int64_t large_neg64 = -5000000000000000000;
static_assert(cast_integer_safe<uint8_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<uint16_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<uint32_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<uint64_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<int8_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<int16_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<int32_t>(large_neg64) == std::nullopt);
static_assert(cast_integer_safe<int64_t>(large_neg64) == -5000000000000000000);

constexpr uint64_t small_pos64 = 1;
static_assert(cast_integer_safe<uint8_t>(small_pos64) == 1);
static_assert(cast_integer_safe<uint16_t>(small_pos64) == 1);
static_assert(cast_integer_safe<uint32_t>(small_pos64) == 1);
static_assert(cast_integer_safe<uint64_t>(small_pos64) == 1);
static_assert(cast_integer_safe<int8_t>(small_pos64) == 1);
static_assert(cast_integer_safe<int16_t>(small_pos64) == 1);
static_assert(cast_integer_safe<int32_t>(small_pos64) == 1);
static_assert(cast_integer_safe<int64_t>(small_pos64) == 1);

constexpr int64_t small_neg64 = -1;
static_assert(cast_integer_safe<uint8_t>(small_neg64) == std::nullopt);
static_assert(cast_integer_safe<uint16_t>(small_neg64) == std::nullopt);
static_assert(cast_integer_safe<uint32_t>(small_neg64) == std::nullopt);
static_assert(cast_integer_safe<uint64_t>(small_neg64) == std::nullopt);
static_assert(cast_integer_safe<int8_t>(small_neg64) == -1);
static_assert(cast_integer_safe<int16_t>(small_neg64) == -1);
static_assert(cast_integer_safe<int32_t>(small_neg64) == -1);
static_assert(cast_integer_safe<int64_t>(small_neg64) == -1);

// boundaries
constexpr uint8_t max_u8 = std::numeric_limits<uint8_t>::max();
static_assert(cast_integer_safe<uint8_t>(max_u8) == 255);
static_assert(cast_integer_safe<uint16_t>(max_u8) == 255);
static_assert(cast_integer_safe<uint32_t>(max_u8) == 255);
static_assert(cast_integer_safe<uint64_t>(max_u8) == 255);
static_assert(cast_integer_safe<int8_t>(max_u8) == std::nullopt);
static_assert(cast_integer_safe<int16_t>(max_u8) == 255);
static_assert(cast_integer_safe<int32_t>(max_u8) == 255);
static_assert(cast_integer_safe<int64_t>(max_u8) == 255);

constexpr uint8_t min_u8 = std::numeric_limits<uint8_t>::min();
static_assert(cast_integer_safe<uint8_t>(min_u8) == 0);
static_assert(cast_integer_safe<uint16_t>(min_u8) == 0);
static_assert(cast_integer_safe<uint32_t>(min_u8) == 0);
static_assert(cast_integer_safe<uint64_t>(min_u8) == 0);
static_assert(cast_integer_safe<int8_t>(min_u8) == 0);
static_assert(cast_integer_safe<int16_t>(min_u8) == 0);
static_assert(cast_integer_safe<int32_t>(min_u8) == 0);
static_assert(cast_integer_safe<int64_t>(min_u8) == 0);

constexpr int8_t max_i8 = std::numeric_limits<int8_t>::max();
static_assert(cast_integer_safe<uint8_t>(max_i8) == 127);
static_assert(cast_integer_safe<uint16_t>(max_i8) == 127);
static_assert(cast_integer_safe<uint32_t>(max_i8) == 127);
static_assert(cast_integer_safe<uint64_t>(max_i8) == 127);
static_assert(cast_integer_safe<int8_t>(max_i8) == 127);
static_assert(cast_integer_safe<int16_t>(max_i8) == 127);
static_assert(cast_integer_safe<int32_t>(max_i8) == 127);
static_assert(cast_integer_safe<int64_t>(max_i8) == 127);

constexpr int8_t min_i8 = std::numeric_limits<int8_t>::min();
static_assert(cast_integer_safe<uint8_t>(min_i8) == std::nullopt);
static_assert(cast_integer_safe<uint16_t>(min_i8) == std::nullopt);
static_assert(cast_integer_safe<uint32_t>(min_i8) == std::nullopt);
static_assert(cast_integer_safe<uint64_t>(min_i8) == std::nullopt);
static_assert(cast_integer_safe<int8_t>(min_i8) == -128);
static_assert(cast_integer_safe<int16_t>(min_i8) == -128);
static_assert(cast_integer_safe<int32_t>(min_i8) == -128);
static_assert(cast_integer_safe<int64_t>(min_i8) == -128);

// same size, different sign
constexpr uint8_t above = 200;
static_assert(cast_integer_safe<int8_t>(above) == std::nullopt);

constexpr uint8_t within_unsigned = 100;
static_assert(cast_integer_safe<int8_t>(within_unsigned) == 100);

constexpr int8_t within_signed = 100;
static_assert(cast_integer_safe<uint8_t>(within_signed) == 100);

constexpr int8_t below = -100;
static_assert(cast_integer_safe<uint8_t>(below) == std::nullopt);
} // namespace detail::int_cast_tests


namespace detail
{
template<typename T>
constexpr inline int64_t min_int_that_fits_in_float()
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);

    // Ideally, this function would just be the following one-liner, but
    // overflow and constexpr rules mean this doesn't work correctly in all cases.
    //return static_cast<int64_t>(ceil(numeric_limits<T>::min()));

    if constexpr (std::is_same_v<T, float>)
    {
        // this constant value is only valid if floats are 32 bits
        static_assert(sizeof(float) == sizeof(int32_t));
        return static_cast<int64_t>(-0x7fffffff) - 1;
    }
    else
    {
        // this constant value is only valid if doubles are 64 bits
        static_assert(sizeof(double) == sizeof(int64_t));
        return static_cast<int64_t>(-0x4340000000000000);
    }

    //static_assert(std::is_floating_point_v<T>);
    // for negative values (and min is always negative), casting to int truncates correctly.
    // this is effectively doing (int64_t)ceil(numeric_limits<T>::min()), but ceil() isn't constexpr.
    //return (int64_t)std::numeric_limits<T>::min();
}

template<typename T>
constexpr inline int64_t max_int_that_fits_in_float()
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);

    // Ideally, this function would just be the following one-liner, but
    // overflow and constexpr rules mean this doesn't work correctly in all cases.
    //return static_cast<int64_t>(floor(numeric_limits<T>::max()));

    if constexpr (std::is_same_v<T, float>)
    {
        // this constant value is only valid if floats are 32 bits
        static_assert(sizeof(float) == sizeof(int32_t));
        return static_cast<int64_t>(0x7fffffff) + 1;
    }
    else
    {
        // this constant value is only valid if doubles are 64 bits
        static_assert(sizeof(double) == sizeof(int64_t));
        return static_cast<int64_t>(0x4340000000000000);
    }
}

static_assert(static_cast<int64_t>(static_cast<float>(min_int_that_fits_in_float<float>())) == min_int_that_fits_in_float<float>());
static_assert(static_cast<int64_t>(static_cast<double>(min_int_that_fits_in_float<double>())) == min_int_that_fits_in_float<double>());

static_assert(static_cast<int64_t>(static_cast<float>(max_int_that_fits_in_float<float>())) == max_int_that_fits_in_float<float>());
static_assert(static_cast<int64_t>(static_cast<double>(max_int_that_fits_in_float<double>())) == max_int_that_fits_in_float<double>());

} // namespace detail


// Convert any type that qualifies as traits::Integer to a numeric value,
// clamping the value to fit the result type.
template<typename Dst = int64_t, typename Src>
constexpr inline Dst cast_integer_clamped(Src val)
{
    static_assert(is_integer_type<Src>);

    if constexpr (std::is_floating_point_v<Dst>)
    {
        if ((int64_t)val < detail::min_int_that_fits_in_float<Dst>())
        {
            return std::numeric_limits<Dst>::min();
        }
        else if ((int64_t)val > detail::max_int_that_fits_in_float<Dst>())
        {
            return std::numeric_limits<Dst>::max();
        }
        else
        {
            return static_cast<Dst>(val);
        }
    }
    else
    {
        NumericCastResult<Dst> result = cast_integer<Dst, Src>(val);
        if (result.type == OutOfBoundsType::High)
        {
            return std::numeric_limits<Dst>::max();
        }
        else if (result.type == OutOfBoundsType::Low)
        {
            return std::numeric_limits<Dst>::min();
        }
        else if (result.type == OutOfBoundsType::NotANumber)
        {
            return static_cast<Dst>(0);
        }
        else
        {
            return result.value;
        }
    }
}


// Convert any type that qualifies as traits::Float to any numeric value,
// clamping the value to fit the bounds of the result type.
template<typename Dst = double, typename Src>
constexpr inline Dst cast_float_clamped(Src val)
{
    static_assert(is_float_type<Dst> && is_numeric_type<Src>);
    if constexpr (std::is_same_v<Src, tag::Float>)
    {
        return static_cast<Dst>(0);
    }
    else if constexpr (std::is_same_v<Src, Dst>)
    {
        return val;
    }
    else if constexpr (std::is_integral_v<Dst>)
    {
        val = std::round(val);
        if (val > std::numeric_limits<Dst>::max())
        {
            return std::numeric_limits<Dst>::max();
        }
        else if (val < std::numeric_limits<Dst>::min())
        {
            return std::numeric_limits<Dst>::min();
        }
    }

    return static_cast<Dst>(val);
}

} // namespace traits

#if JXC_HAVE_CONCEPTS
// some simple test cases for traits
//static_assert(traits::Null<decltype(default_null)>, "default_null matches the Null trait");
static_assert(traits::Null<decltype(nullptr)>, "nullptr matches the Null trait");
static_assert(traits::Bool<decltype(false)>, "false constant matches the Bool trait");
static_assert(traits::Bool<decltype(true)>, "true constant matches the Bool trait");
static_assert(traits::Bool<tag::Bool>, "Bool tag matches the Bool trait");
//static_assert(traits::Bool<decltype(default_bool)>, "Bool tag matches the Bool trait");
static_assert(traits::Integer<tag::Integer>, "Integer tag matches the Integer trait");
static_assert(traits::Integer<decltype(1234UL)>, "unsigned long constant matches the Integer trait");
static_assert(traits::Float<tag::Float>, "Float tag matches the Float trait");
static_assert(traits::Float<decltype(1.05f)>, "Float constant matches the Float trait");
static_assert(traits::Float<decltype(1.05)>, "Double constant matches the Float trait");
static_assert(std::convertible_to<tag::String, std::string_view>, "tag::String must be convertible to string_view");
static_assert(traits::String<tag::String>, "tag::String is a string");
static_assert(traits::Bytes<std::vector<uint8_t>>, "std::vector<uint8_t> matches the Bytes trait");
static_assert(traits::Array<std::vector<int>, int>, "std::vector<int> matches the Array trait");
#endif // JXC_HAVE_CONCEPTS

namespace traits
{

constexpr inline std::string_view cast_string_to_view(const char* val)
{
    return (val == nullptr) ? std::string_view{ "" } : std::string_view(val);
}

// convert any type that has data() and size() members to a std::string_view
template<JXC_CONCEPT(HasDataAndSize<char>) T>
constexpr inline std::string_view cast_string_to_view(const T& val)
{
    return std::string_view{ val.data(), val.size() };
}

} // namespace traits

} // namespace jxc
