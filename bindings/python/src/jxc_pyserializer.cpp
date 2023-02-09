#include "jxc_pyserializer.h"


namespace jxc
{

PyEncoder::PyEncoder(const SerializerSettings& settings, bool encode_inline, bool sort_keys, bool skip_keys, bool decode_unicode)
    : doc(settings)
    , encode_inline(encode_inline)
    , sort_keys(sort_keys)
    , skip_keys(skip_keys)
    , decode_unicode(decode_unicode)
{
}


void PyEncoder::set_find_encoder_callback(py::object callback)
{
    if (callback.is_none())
    {
        find_encoder_callback = nullptr;
    }
    else
    {
        find_encoder_callback = [callback](py::object value) -> py::object
        {
            return callback(value);
        };
    }
}


void PyEncoder::set_find_fallback_encoder_callback(py::object callback)
{
    if (callback.is_none())
    {
        find_fallback_encoder_callback = nullptr;
    }
    else
    {
        find_fallback_encoder_callback = [callback](py::object value) -> py::object
        {
            return callback(value);
        };
    }
}


void PyEncoder::set_nan_encoder_callback(py::object callback)
{
    if (callback.is_none())
    {
        nan_encoder_callback = nullptr;
    }
    else
    {
        nan_encoder_callback = [callback](Serializer& doc, PyEncoder& enc, FloatLiteralType value)
        {
            py::object self = py::cast(enc, py::return_value_policy::reference);
            callback(self.attr("doc"), self, value);
        };
    }
}


void PyEncoder::encode_sequence(py::sequence val)
{
    doc.array_begin();
    for (const auto& item : val)
    {
        encode_value(item);
    }
    doc.array_end();
}


bool PyEncoder::encode_dict_key(py::object key)
{
    if (key.is_none())
    {
        doc.value_null();
        return true;
    }
    else if (py::isinstance<py::int_>(key))
    {
        doc.value_int(py::cast<int64_t>(key));
        return true;
    }
    else if (py::isinstance<py::str>(key))
    {
        std::string_view key_str = py::cast<std::string_view>(key);
        doc.identifier_or_string(key_str);
        return true;
    }

    return false;
}


void PyEncoder::encode_dict(py::dict val)
{
    doc.object_begin();
    if (sort_keys)
    {
        py::list keys;
        for (const auto& key_handle : val.attr("keys")())
        {
            keys.append(key_handle);
        }
        keys.attr("sort")();

        for (const auto& key_handle : keys)
        {
            py::object key = py::reinterpret_borrow<py::object>(key_handle);
            if (!encode_dict_key(key))
            {
                if (skip_keys)
                {
                    continue;
                }
                else
                {
                    throw py::type_error(jxc::format("Failed encoding dict key {}", py::cast<std::string>(py::repr(key))));
                }
            }

            doc.object_sep();
            py::object item = py::reinterpret_borrow<py::object>(val[key]);
            encode_value(item);
        }
    }
    else
    {
        for (const auto& pair : val)
        {
            py::object key = py::reinterpret_borrow<py::object>(pair.first);
            if (!encode_dict_key(key))
            {
                if (skip_keys)
                {
                    continue;
                }
                else
                {
                    throw py::type_error(jxc::format("Failed encoding dict key {}", py::cast<std::string>(py::repr(key))));
                }
            }

            doc.object_sep();
            py::object item = py::reinterpret_borrow<py::object>(pair.second);
            encode_value(item);
        }
    }
    doc.object_end();
}


void PyEncoder::encode_value(py::object val)
{
    auto try_use_encoder = [this](const FindEncoderFunc& callback, py::object py_value) -> bool
    {
        if (callback != nullptr)
        {
            py::object encoder_func = callback(py_value);
            if (!encoder_func.is_none())
            {
                py::object self = py::cast(*this, py::return_value_policy::reference);
                encoder_func(self.attr("doc"), self, py_value);
                return true;
            }
        }

        return false;
    };

    // do we have an encoder function for this value?
    if (try_use_encoder(find_encoder_callback, val))
    {
        return;
    }

    // try python builtin types
    if (val.is_none())
    {
        doc.value_null();
        return;
    }
    else if (py::isinstance<py::bool_>(val))
    {
        doc.value_bool(py::cast<bool>(val));
        return;
    }
    else if (py::isinstance<py::int_>(val))
    {
        doc.value_int(py::cast<int64_t>(val));
        return;
    }
    else if (py::isinstance<py::float_>(val))
    {
        const double float_val = py::cast<double>(val);
        const FloatLiteralType float_type = get_float_literal_type(float_val);
        if (float_type == FloatLiteralType::Finite)
        {
            doc.value_float(float_val);
        }
        else if (nan_encoder_callback != nullptr)
        {
            nan_encoder_callback(doc, *this, float_type);
        }
        else
        {
            switch (float_type)
            {
            case FloatLiteralType::NotANumber:
                throw py::value_error("Failed to encode NaN");
            case FloatLiteralType::PosInfinity:
                throw py::value_error("Failed to encode Infinity");
            case FloatLiteralType::NegInfinity:
                throw py::value_error("Failed to encode -Infinity");
            default:
                JXC_ASSERTF(false, "Invalid float literal type {}", static_cast<int64_t>(float_type));
                break;
            }
        }
        return;
    }
    else if (py::isinstance<py::str>(val))
    {
        doc.value_string(py::cast<std::string_view>(val), StringQuoteMode::Auto, decode_unicode);
        return;
    }
    else if (py::isinstance<py::bytes>(val) || py::isinstance<py::bytearray>(val))
    {
        std::string_view bytes_data = py::cast<std::string_view>(val);
        doc.value_bytes(reinterpret_cast<const uint8_t*>(bytes_data.data()), bytes_data.size());
        return;
    }
    else if (detail::is_python_datetime(val))
    {
        doc.value_datetime(py::cast<jxc::DateTime>(val));
        return;
    }
    else if (detail::is_python_date(val))
    {
        doc.value_date(py::cast<jxc::Date>(val));
        return;
    }
    else if (py::isinstance<py::list>(val) || py::isinstance<py::tuple>(val))
    {
        encode_sequence(val);
        return;
    }
    else if (py::isinstance<py::dict>(val))
    {
        encode_dict(val);
        return;
    }

    // see if the type has an inline encoder (_jxc_encode method)
    if (encode_inline && py::hasattr(val, "_jxc_encode"))
    {
        // value has a _jxc_encode method
        py::object self = py::cast(*this, py::return_value_policy::reference);
        val.attr("_jxc_encode")(self.attr("doc"), self);
        return;
    }
    
    // do we have a fallback encoder?
    if (try_use_encoder(find_fallback_encoder_callback, val))
    {
        return;
    }

    throw py::type_error(jxc::format("Failed encoding value {}", py::cast<std::string>(py::repr(val))));
}

} // namespace jxc
