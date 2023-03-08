#pragma once
#include "jxc/jxc.h"
#include "jxc/jxc_util.h"
#include "jxc/jxc_serializer.h"

#include "jxc_python.h"


namespace jxc
{

class PyOutputBuffer : public IOutputBuffer
{
    std::vector<char> buf;

public:
    PyOutputBuffer()
        : buf()
    {
    }

    virtual ~PyOutputBuffer()
    {
    }

    virtual void write(const char* value, size_t value_len) override
    {
        const size_t start_idx = buf.size();
        buf.resize(buf.size() + value_len);
        JXC_MEMCPY(&buf[start_idx], value_len, value, value_len);
    }

    inline void clear()
    {
        buf.clear();
    }

    inline py::str get_result() const
    {
        return py::str(buf.data(), buf.size());
    }

    inline py::bytes get_result_bytes() const
    {
        return py::bytes(buf.data(), buf.size());
    }
};


class JXC_NOEXPORT PySerializer : public Serializer
{
public:
    using FindEncoderFunc = std::function<py::object(py::object)>;

private:
    PyOutputBuffer py_output_buffer;
    FindEncoderFunc override_encoder_callback;
    FindEncoderFunc default_primary_encoder_callback;
    FindEncoderFunc default_secondary_encoder_callback;

    static IOutputBuffer* init_output_buffer(PyOutputBuffer& out_buffer)
    {
        out_buffer = PyOutputBuffer();
        return &out_buffer;
    }

public:
    bool encode_inline = false;
    bool sort_keys = false;
    bool decode_unicode = false;

    PySerializer(const SerializerSettings& settings, bool encode_inline, bool sort_keys, bool decode_unicode)
        : encode_inline(encode_inline)
        , sort_keys(sort_keys)
        , decode_unicode(decode_unicode)
        , Serializer(init_output_buffer(py_output_buffer), settings)
    {
    }

    virtual ~PySerializer()
    {
    }

    py::str get_result()
    {
        flush();
        return py_output_buffer.get_result();
    }

    py::bytes get_result_bytes()
    {
        flush();
        return py_output_buffer.get_result_bytes();
    }

    void set_override_encoder_callback(py::object callback);
    void set_default_primary_encoder_callback(py::object callback);
    void set_default_secondary_encoder_callback(py::object callback);

    PySerializer& object_key(py::object key);

    PySerializer& value_sequence(py::sequence val, std::string_view separator = std::string_view{});
    PySerializer& value_dict(py::dict val, std::string_view separator = std::string_view{});
    PySerializer& value_auto(py::object val);
};

} // namespace jxc
