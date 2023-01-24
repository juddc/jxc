#pragma once
#include "jxc/jxc_format.h"
#include "jxc/jxc_string.h"

#include "pybind11/pybind11.h"
namespace py = pybind11;


JXC_BEGIN_NAMESPACE(jxc::detail)

inline size_t python_index(int64_t idx, size_t container_size)
{
    JXC_DEBUG_ASSERT(container_size < (size_t)std::numeric_limits<int64_t>::max());

    // allow python-style negative indexing, where an index of -1 means the last item, -2 means the second to last item, etc.
    if (idx < 0)
    {
        idx += static_cast<int64_t>(container_size);
    }

    // bounds check
    if (idx < 0 || idx >= static_cast<int64_t>(container_size))
    {
        throw py::index_error(jxc::format("Index {} out of range for container of size {}", idx, container_size));
    }

    return static_cast<size_t>(idx);
}

JXC_END_NAMESPACE(jxc::detail)


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

