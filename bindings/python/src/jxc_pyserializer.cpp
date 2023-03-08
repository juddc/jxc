#include "jxc_pyserializer.h"


namespace jxc
{

void PySerializer::set_override_encoder_callback(py::object callback)
{
    if (callback.is_none())
    {
        override_encoder_callback = nullptr;
    }
    else
    {
        override_encoder_callback = [callback](py::object value) -> py::object
        {
            return callback(value);
        };
    }
}


void PySerializer::set_default_primary_encoder_callback(py::object callback)
{
    if (callback.is_none())
    {
        default_primary_encoder_callback = nullptr;
    }
    else
    {
        default_primary_encoder_callback = [callback](py::object value) -> py::object
        {
            return callback(value);
        };
    }
}


void PySerializer::set_default_secondary_encoder_callback(py::object callback)
{
    if (callback.is_none())
    {
        default_secondary_encoder_callback = nullptr;
    }
    else
    {
        default_secondary_encoder_callback = [callback](py::object value) -> py::object
        {
            return callback(value);
        };
    }
}


PySerializer& PySerializer::object_key(py::object key)
{
    if (key.is_none())
    {
        value_null();
    }
    else if (py::isinstance<py::int_>(key))
    {
        value_int(py::cast<int64_t>(key));
    }
    else if (py::isinstance<py::str>(key))
    {
        std::string_view key_str = py::cast<std::string_view>(key);
        identifier_or_string(key_str);
    }
    else
    {
        throw py::type_error(jxc::format("Invalid type {} for dict key", py::cast<std::string>(py::repr(py::type(key)))));
    }
    return *this;
}


PySerializer& PySerializer::value_sequence(py::sequence val, std::string_view separator)
{
    array_begin(separator);
    for (const auto& item : val)
    {
        value_auto(item);
    }
    array_end();
    return *this;
}


PySerializer& PySerializer::value_dict(py::dict val, std::string_view separator)
{
    object_begin(separator);
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
            object_key(key);
            object_sep();
            value_auto(py::reinterpret_borrow<py::object>(val[key]));
        }
    }
    else
    {
        for (const auto& pair : val)
        {
            object_key(py::reinterpret_borrow<py::object>(pair.first));
            object_sep();
            value_auto(py::reinterpret_borrow<py::object>(pair.second));
        }
    }
    object_end();

    return *this;
}


PySerializer& PySerializer::value_auto(py::object val)
{
    auto try_use_encoder = [this](const FindEncoderFunc& callback, py::object py_value) -> bool
    {
        py::object encoder_func = callback(py_value);
        if (!encoder_func.is_none())
        {
            py::object self = py::cast(*this, py::return_value_policy::reference);
            encoder_func(self, py_value);
            return true;
        }
        return false;
    };

    // check if we have an override encoder first - this lookup function is called before trying any of the standard types.
    if (override_encoder_callback && try_use_encoder(override_encoder_callback, val))
    {
        return *this;
    }

    // try python builtin types
    if (val.is_none())
    {
        value_null();
        return *this;
    }
    else if (py::isinstance<py::bool_>(val))
    {
        value_bool(py::cast<bool>(val));
        return *this;
    }
    else if (py::isinstance<py::int_>(val))
    {
        value_int(py::cast<int64_t>(val));
        return *this;
    }
    else if (py::isinstance<py::float_>(val))
    {
        const double float_val = py::cast<double>(val);

        switch (get_float_literal_type(float_val))
        {
        case FloatLiteralType::NotANumber:
            value_nan();
            return *this;
        case FloatLiteralType::PosInfinity:
            value_inf();
            return *this;
        case FloatLiteralType::NegInfinity:
            value_inf(true);
            return *this;
        default:
            break;
        }

        value_float(float_val);
        return *this;
    }
    else if (py::isinstance<py::str>(val))
    {
        value_string(py::cast<std::string_view>(val), StringQuoteMode::Auto, decode_unicode);
        return *this;
    }
    else if (py::isinstance<py::bytes>(val) || py::isinstance<py::bytearray>(val))
    {
        std::string_view bytes_data = py::cast<std::string_view>(val);
        value_bytes(reinterpret_cast<const uint8_t*>(bytes_data.data()), bytes_data.size());
        return *this;
    }
    else if (detail::is_python_datetime(val))
    {
        value_datetime(py::cast<jxc::DateTime>(val));
        return *this;
    }
    else if (detail::is_python_date(val))
    {
        value_date(py::cast<jxc::Date>(val));
        return *this;
    }
    else if (py::isinstance<py::list>(val) || py::isinstance<py::tuple>(val))
    {
        value_sequence(val);
        return *this;
    }
    else if (py::isinstance<py::dict>(val))
    {
        value_dict(val);
        return *this;
    }

    // see if the type has an inline encoder (_jxc_encode method)
    if (encode_inline && py::hasattr(val, "_jxc_encode"))
    {
        // value has a _jxc_encode method
        py::object self = py::cast(*this, py::return_value_policy::reference);
        val.attr("_jxc_encode")(self);
        return *this;
    }

    // check if we have a default - this lookup function is called after trying all of the standard types.
    if (default_primary_encoder_callback && try_use_encoder(default_primary_encoder_callback, val))
    {
        return *this;
    }

    // check if we have a secondary default - this lookup function is called after trying all of the standard types.
    if (default_secondary_encoder_callback && try_use_encoder(default_secondary_encoder_callback, val))
    {
        return *this;
    }

    throw py::type_error(jxc::format("Failed encoding value {}", py::cast<std::string>(py::repr(val))));
}

} // namespace jxc
