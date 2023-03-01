#pragma once
#include "jxc/jxc.h"
#include "jxc/jxc_util.h"
#include "jxc/jxc_serializer.h"
#include "jxc_cpp/jxc_map.h"
#include "jxc_cpp/jxc_value.h"
#include <vector>


JXC_BEGIN_NAMESPACE(jxc)


class DocumentSerializer
{
    StringOutputBuffer buf;
    Serializer ar;

public:
    DocumentSerializer(const SerializerSettings& settings = SerializerSettings{})
        : buf()
        , ar(&buf, settings)
    {
    }

    inline Serializer& serialize(const Value& val) { serialize_value(ar, val); return ar; }
    inline Serializer& get_serializer() { return ar; }
    inline std::string to_string() { ar.flush(); return buf.to_string(); }

private:
    static void serialize_null(Serializer& doc, const Value& val);
    static void serialize_bool(Serializer& doc, const Value& val);
    static void serialize_signed_integer(Serializer& doc, const Value& val);
    static void serialize_unsigned_integer(Serializer& doc, const Value& val);
    static void serialize_float(Serializer& doc, const Value& val);
    static void serialize_string(Serializer& doc, const Value& val);
    static void serialize_bytes(Serializer& doc, const Value& val);
    static void serialize_array(Serializer& doc, const Value& val);
    static void serialize_object(Serializer& doc, const Value& val);

public:
    static void serialize_value(Serializer& doc, const Value& val);
};


JXC_BEGIN_NAMESPACE(detail)

struct ValueParser
{
    using MakeValueFunc = const std::function<Value(ValueParser&, ElementType, const Token&, TokenView)>;

    JumpParser& parser;
    ErrorInfo& parse_error;
    bool try_return_view = false;
    MakeValueFunc make_value_callback;

private:
    template<typename T>
    inline Value make_value_internal(const T& val, TokenView anno)
    {
        Value result(val);
        if (anno)
        {
            result.set_annotation(anno, try_return_view);
        }
        return result;
    }

    template<typename T>
    inline Value make_value_internal(const T& val, std::string_view number_suffix, TokenView anno)
    {
        Value result(val, number_suffix);
        if (anno)
        {
            result.set_annotation(anno, try_return_view);
        }
        return result;
    }

    inline Value parse_value_internal(const Element& ele)
    {
        return make_value_callback
            ? make_value_callback(*this, ele.type, ele.token, ele.annotation)
            : parse_value(ele.type, ele.token, ele.annotation);
    }

public:
    ValueParser(JumpParser& parser, ErrorInfo& parse_error, bool try_return_view = false, const MakeValueFunc& make_value_callback = nullptr)
        : parser(parser)
        , parse_error(parse_error)
        , try_return_view(try_return_view)
        , make_value_callback(make_value_callback)
    {
    }

    Value parse_number(const Token& tok, TokenView annotation);
    Value parse_bool(const Token& tok, TokenView annotation);
    Value parse_null(const Token& tok, TokenView annotation);
    Value parse_string(const Token& tok, TokenView annotation);
    Value parse_bytes(const Token& tok, TokenView annotation);
    Value parse_array(TokenView annotation);
    Value parse_expression_as_string(TokenView annotation);
    Value parse_expression_as_array(TokenView annotation);
    Value parse_key(const Token& tok);
    Value parse_object(TokenView annotation);
    Value parse_value(ElementType element_type, const Token& tok, TokenView annotation, bool expr_as_string = false);

    Value parse(const Element& ele);
};

JXC_END_NAMESPACE(detail)


class Document
{
    friend class Value;

private:
    std::string buffer;
    JumpParser parser;
    ErrorInfo err;

    StringMap<std::vector<Token>> annotation_cache;

public:
    explicit Document(std::string_view in_buffer);

private:
    // Makes a deduplicated copy of an annotation that's owned by this Document, then returns
    // a new TokenView pointing to our local copy.
    // This is needed because the parser does not handle memory allocation or ownership at all,
    // so we can't just store an existing TokenView - we need to cache it ourselves.
    // Because annotations tend to be duplicated all over a document, deduplicating can
    // potentially save a large amount of memory.
    TokenView copy_annotation(TokenView anno);

private:
    bool advance();

public:
    Value parse();

    Value parse_to_owned();

    const std::string& get_buffer() const { return buffer; }

    inline bool has_error() const { return err.is_err; }

    inline ErrorInfo& get_error() { return err; }
    inline const ErrorInfo& get_error() const { return err; }
};


/// Standalone parsing function. If a parse error occurs, returns an invalid value.
/// Returns the parse error (if any) in out_error.
/// This function always returns a fully owned Value.
Value parse(std::string_view jxc_string, ErrorInfo& out_error, bool try_return_view = false);


/// Standalone parsing function. If a parse error occurs, returns an invalid value.
/// This function always returns a fully owned Value.
inline Value parse(std::string_view jxc_string, bool try_return_view = false)
{
    ErrorInfo err;
    return jxc::parse(jxc_string, err, try_return_view);
}


/// Standalone serialization function.
/// Serializes a Value into a jxc string, using the specified serializer settings.
std::string serialize(const Value& val, const SerializerSettings& settings = SerializerSettings{});


JXC_END_NAMESPACE(jxc)
