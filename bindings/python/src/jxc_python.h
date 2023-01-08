#pragma once
#include "jxc/jxc_string.h"

#include "pybind11/pybind11.h"
namespace py = pybind11;


namespace PYBIND11_NAMESPACE
{
namespace detail
{
template<>
struct type_caster<jxc::FlexString>
{
public:
    PYBIND11_TYPE_CASTER(jxc::FlexString, const_name("str"));

    // Conversion part 1 (Python->C++)
    // convert a PyObject into a jxc::FlexString instance or return false upon failure.
    bool load(handle src, bool /* implicit_conversion_allowed */)
    {
        Py_ssize_t str_len = 0;
        const char* str_ptr = PyUnicode_AsUTF8AndSize(src.ptr(), &str_len);
        if (str_ptr != nullptr && str_len >= 0)
        {
            value.set_owned(str_ptr, static_cast<size_t>(str_len));
            return true;
        }
        return false;
    }

    // Conversion part 2 (C++ -> Python)
    // convert an jxc::FlexString instance into a Python object.
    static handle cast(const jxc::FlexString& src, return_value_policy /* policy */, handle /* parent */)
    {
        return PyUnicode_FromStringAndSize(src.data(), src.size());
    }
};
} // namespace detail
} // namespace PYBIND11_NAMESPACE

