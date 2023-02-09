#pragma once
#include "jxc/jxc_format.h"
#include "jxc/jxc_string.h"

#include "pybind11/pybind11.h"
namespace py = pybind11;

// Python DateTime header
#include <datetime.h>


// For some reason, the PyDateTime_FromDateAndTime macro doesn't expose the tzinfo argument,
// so this is the same macro but with that argument added.
// See also:
//  * https://github.com/python/cpython/issues/74341
//  * https://github.com/python/cpython/issues/83785
#define JXC_PyDateTime_FromDateAndTimeAndTimezone(year, month, day, hour, min, sec, usec, tzinfo) \
    PyDateTimeAPI->DateTime_FromDateAndTime(year, month, day, hour, \
        min, sec, usec, tzinfo, PyDateTimeAPI->DateTimeType)


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

bool is_python_date(py::handle value);
bool is_python_datetime(py::handle value);

inline py::object create_tzinfo_from_offset(int8_t tz_hour, int8_t tz_minute)
{
    PyObject* offset_ptr = PyDelta_FromDSU(0, (tz_hour * 60 * 60) + (tz_minute * 60), 0);
    if (!offset_ptr)
    {
        throw py::cast_error("Failed to create time zone offset delta object");
    }
    py::object offset = py::reinterpret_steal<py::object>(offset_ptr);

    PyObject* tzinfo_ptr = PyTimeZone_FromOffset(offset.ptr());
    if (!tzinfo_ptr)
    {
        throw py::cast_error("Failed to create tzinfo");
    }
    return py::reinterpret_steal<py::object>(tzinfo_ptr);
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

template<>
struct type_caster<jxc::Date>
{
public:
    PYBIND11_TYPE_CASTER(jxc::Date, const_name("datetime.date"));

    // Conversion part 1 (Python->C++)
    // convert a PyObject into a jxc::Date instance or return false upon failure.
    bool load(handle src, bool /* implicit_conversion_allowed */)
    {
        // lazy initialization for the PyDateTime API
        if (!PyDateTimeAPI)
        {
            PyDateTime_IMPORT;
        }

        if (src && PyDate_Check(src.ptr()))
        {
            value.year = PyDateTime_GET_YEAR(src.ptr()); // - 1900;
            value.month = PyDateTime_GET_MONTH(src.ptr()); // - 1;
            value.day = PyDateTime_GET_DAY(src.ptr());
            return true;
        }
        return false;
    }

    // Conversion part 2 (C++ -> Python)
    // convert an jxc::FlexString instance into a Python object.
    static handle cast(const jxc::Date& src, return_value_policy /* policy */, handle /* parent */)
    {
        // lazy initialization for the PyDateTime API
        if (!PyDateTimeAPI)
        {
            PyDateTime_IMPORT;
        }

        return PyDate_FromDate(
            src.year, // + 1900,
            src.month, // + 1,
            src.day);
    }
};

template<>
struct type_caster<jxc::DateTime>
{
public:
    PYBIND11_TYPE_CASTER(jxc::DateTime, const_name("datetime.datetime"));

    // Conversion part 1 (Python->C++)
    // convert a PyObject into a jxc::DateTime instance or return false upon failure.
    bool load(handle src, bool /* implicit_conversion_allowed */)
    {
        // lazy initialization for the PyDateTime API
        if (!PyDateTimeAPI)
        {
            PyDateTime_IMPORT;
        }

        if (src && PyDateTime_Check(src.ptr()))
        {
            value.year = PyDateTime_GET_YEAR(src.ptr()); // - 1900;
            value.month = PyDateTime_GET_MONTH(src.ptr()); // - 1;
            value.day = PyDateTime_GET_DAY(src.ptr());
            value.hour = PyDateTime_DATE_GET_HOUR(src.ptr());
            value.minute = PyDateTime_DATE_GET_MINUTE(src.ptr());
            value.second = PyDateTime_DATE_GET_SECOND(src.ptr());
            value.nanosecond = PyDateTime_DATE_GET_MICROSECOND(src.ptr()) * 1000;  // convert microsecond -> nanosecond
            return true;
        }
        return false;
    }

    // Conversion part 2 (C++ -> Python)
    // convert an jxc::FlexString instance into a Python object.
    static handle cast(const jxc::DateTime& src, return_value_policy /* policy */, handle /* parent */)
    {
        // lazy initialization for the PyDateTime API
        if (!PyDateTimeAPI)
        {
            PyDateTime_IMPORT;
        }

        if (src.is_timezone_local())
        {
            return PyDateTime_FromDateAndTime(
                src.year, // + 1900,
                src.month, // + 1,
                src.day,
                src.hour,
                src.minute,
                src.second,
                src.nanosecond / 1000);
        }
        else if (src.is_timezone_utc())
        {
            return JXC_PyDateTime_FromDateAndTimeAndTimezone(
                src.year, // + 1900,
                src.month, // + 1,
                src.day,
                src.hour,
                src.minute,
                src.second,
                src.nanosecond / 1000,
                PyDateTimeAPI->TimeZone_UTC);
        }
        else
        {
            py::object tzinfo = jxc::detail::create_tzinfo_from_offset(src.tz_hour, src.tz_minute);
            return JXC_PyDateTime_FromDateAndTimeAndTimezone(
                src.year, // + 1900,
                src.month, // + 1,
                src.day,
                src.hour,
                src.minute,
                src.second,
                src.nanosecond / 1000,  // convert nanoseconds -> microsecond
                tzinfo.ptr());
        }
    }
};

} // namespace detail
} // namespace PYBIND11_NAMESPACE

