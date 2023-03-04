#pragma once
#include "jxc/jxc_format.h"
#include "jxc/jxc_string.h"

#include "pybind11/pybind11.h"
namespace py = pybind11;

// Python DateTime header
#include <datetime.h>


#if PY_MAJOR_VERSION < 3 || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 8)
#error Unsupported Python version. JXC supports Python 3.8+
#endif


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

inline py::object create_tzinfo_from_offset(int8_t tz_hour, uint8_t tz_minute)
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


inline void get_tzinfo_offset(py::handle datetime, const py::object& tzinfo, int8_t& out_tz_hour, uint8_t& out_tz_minute)
{
    out_tz_hour = 0;
    out_tz_minute = 0;

    auto make_error_prefix = [&datetime, &tzinfo]() -> std::string
    {
        return jxc::format("Failed getting time zone offset from datetime {} and tzinfo {}",
            (datetime.ptr() != nullptr) ? py::cast<std::string>(py::repr(datetime)) : std::string("null"),
            (tzinfo.ptr() != nullptr) ? py::cast<std::string>(py::repr(tzinfo)) : std::string("null"));
    };

    if (PyTZInfo_Check(tzinfo.ptr()) == 0)
    {
        throw py::cast_error(jxc::format("{}: tzinfo is not a valid timezone", make_error_prefix()));
    }

    py::object offset_delta;

    try
    {
        offset_delta = tzinfo.attr("utcoffset")(datetime);
    }
    catch (const py::error_already_set& e)
    {
        throw py::cast_error(jxc::format("{}: {}", make_error_prefix(), e.what()));
    }
    
    if (PyDelta_Check(offset_delta.ptr()) == 0)
    {
        throw py::cast_error(jxc::format("{}: tzinfo.utcoffset() did not return a valid timedelta object", make_error_prefix()));
    }

    // Note that negative timedeltas are stored by using negative values for the days value only.
    // eg. timedelta(seconds=-5) is stored as datetime.timedelta(days=-1, seconds=86395)
    const int64_t delta_days = static_cast<int64_t>(PyDateTime_DELTA_GET_DAYS(offset_delta.ptr()));
    const int64_t delta_seconds = static_cast<int64_t>(PyDateTime_DELTA_GET_SECONDS(offset_delta.ptr()));

    // zero offset means UTC time
    if (delta_days == 0 && delta_seconds == 0)
    {
        return;
    }

    // valid range taken from python3.10/datetime.h
    JXC_DEBUG_ASSERT(delta_seconds >= 0 && delta_seconds < 24*3600);

    static constexpr int64_t seconds_per_hour = 60 * 60;
    int64_t offset_seconds = (delta_days * 24 * seconds_per_hour) + delta_seconds;

    bool is_negative = false;
    if (offset_seconds < 0)
    {
        is_negative = true;
        offset_seconds = -offset_seconds;
    }

    while (offset_seconds >= seconds_per_hour)
    {
        offset_seconds -= seconds_per_hour;
        out_tz_hour += 1;
    }

    while (offset_seconds >= 60)
    {
        offset_seconds -= 60;
        out_tz_minute += 1;
    }

    if (is_negative)
    {
        out_tz_hour = -out_tz_hour;
    }

    if (out_tz_hour <= -24 || out_tz_hour >= 24 || out_tz_minute >= 60)
    {
        throw py::cast_error(jxc::format("{}: timedelta {} out of range", make_error_prefix(),
            py::cast<std::string>(py::repr(offset_delta))));
    }
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


// backport PyDateTime_DATE_GET_TZINFO from Python 3.10 to support Python 3.8 and 3.9
#if !defined(PyDateTime_DATE_GET_TZINFO) && PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 10
#if !defined(_PyDateTime_HAS_TZINFO)
#error Unsupported Python version (missing macros _PyDateTime_HAS_TZINFO and PyDateTime_DATE_GET_TZINFO)
#endif
#define PyDateTime_DATE_GET_TZINFO(o) (_PyDateTime_HAS_TZINFO(o) ? ((PyDateTime_DateTime *)(o))->tzinfo : Py_None)
#endif

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
            py::object tzinfo = py::reinterpret_borrow<py::object>(PyDateTime_DATE_GET_TZINFO(src.ptr()));
            if (tzinfo.is_none())
            {
                // no tzinfo indicates this is a local time
                value.tz_local = true;
            }
            else
            {
                value.tz_local = false;
                uint8_t tz_minute = 0;
                jxc::detail::get_tzinfo_offset(src, tzinfo, value.tz_hour, tz_minute);
                JXC_DEBUG_ASSERTF(value.tz_hour > -24 && value.tz_hour < 24 && tz_minute < 60,
                    "Invalid time zone offset {}:{}", value.tz_hour, tz_minute);
                value.tz_minute = tz_minute;
            }
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

