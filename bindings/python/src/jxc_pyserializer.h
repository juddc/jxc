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
    PyOutputBuffer() : buf() {}
    virtual ~PyOutputBuffer() {}
    void write(const char* value, size_t value_len) override
    {
        const size_t start_idx = buf.size();
        buf.resize(buf.size() + value_len);
        JXC_MEMCPY(&buf[start_idx], value_len, value, value_len);
    }
    void clear() { buf.clear(); }
    py::str get_result() const { return py::str(buf.data(), buf.size()); }
    py::bytes get_result_bytes() const { return py::bytes(buf.data(), buf.size()); }
};


// This wrapper is only needed to force the lifetime of the
// output buffer to match the lifetime of the serializer
class PySerializer : public Serializer
{
    PyOutputBuffer py_output_buffer;

public:
    PySerializer(const SerializerSettings& settings)
        : py_output_buffer()
        , Serializer(&py_output_buffer, settings)
    {
    }

    virtual ~PySerializer() {}

    py::str get_result() { flush(); return py_output_buffer.get_result(); }
    py::bytes get_result_bytes() { flush(); return py_output_buffer.get_result_bytes(); }
};


class JXC_NOEXPORT PyEncoder
{
public:
    using FindEncoderFunc = std::function<py::object(py::object)>;

private:
    PySerializer doc;
    bool encode_inline = false;
    bool sort_keys = false;
    bool skip_keys = false;
    bool decode_unicode = false;
    FindEncoderFunc find_encoder_callback;
    FindEncoderFunc find_fallback_encoder_callback;

    bool encode_dict_key(py::object key);

public:
    PyEncoder(const SerializerSettings& settings, bool encode_inline, bool sort_keys, bool skip_keys, bool decode_unicode);
    void set_find_encoder_callback(py::object callback);
    void set_find_fallback_encoder_callback(py::object callback);

    void encode_sequence(py::sequence val);
    void encode_dict(py::dict val);
    void encode_value(py::object val);

    PySerializer& get_serializer() { return doc; }
};

} // namespace jxc
