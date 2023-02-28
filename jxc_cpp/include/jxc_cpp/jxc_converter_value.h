#pragma once
#include "jxc_cpp/jxc_converter.h"
#include "jxc_cpp/jxc_value.h"
#include "jxc_cpp/jxc_document.h"


JXC_BEGIN_NAMESPACE(jxc)

template<>
struct Converter<Value>
{
    using value_type = Value;

    static std::string get_annotation()
    {
        return "any";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        DocumentSerializer::serialize_value(doc, value);
    }

    static value_type parse(conv::Parser& parser, TokenSpan anno)
    {
        ErrorInfo err;
        value_type result = detail::ValueParser(parser, err).parse(parser.value());
        if (result.is_invalid() && err.is_err)
        {
            throw parse_error("Failed to parse value", err);
        }
        return result;
    }
};

JXC_END_NAMESPACE(jxc)
