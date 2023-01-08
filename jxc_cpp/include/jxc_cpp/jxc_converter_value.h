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
        return "value";
    }

    static void serialize(Serializer& doc, const value_type& value)
    {
        DocumentSerializer::serialize_value(doc, value);
    }

    static value_type parse(JumpParser& parser)
    {
        ErrorInfo err;
        value_type result = detail::ValueParser(parser, err).parse(parser.value());
        if (result.is_invalid() && err.is_err)
        {
            throw parse_error(jxc::format("Failed to parse value: {}", err.to_string()));
        }
        return result;
    }
};

JXC_END_NAMESPACE(jxc)
