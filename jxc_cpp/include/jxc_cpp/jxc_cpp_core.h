#pragma once
#include <string>
#include <string_view>
#include "jxc/jxc.h"


JXC_BEGIN_NAMESPACE(jxc)


class parse_error : public std::runtime_error
{
    ErrorInfo err;

    static std::string format_message(const std::string& msg, const ErrorInfo& err_info)
    {
        if (msg.size() > 0 && err_info.message.size() > 0)
        {
            return jxc::format("{}: {}", msg, err_info.message);
        }
        else if (msg.size() > 0)
        {
            return msg;
        }
        else if (err_info.message.size() > 0)
        {
            return err_info.to_string();
        }
        return std::string();
    }

public:
    explicit parse_error(const std::string& msg, size_t start_idx = invalid_idx, size_t end_idx = invalid_idx)
        : std::runtime_error(format_message(std::string{}, ErrorInfo{ msg, start_idx, end_idx }))
        , err(msg, start_idx, end_idx)
    {
    }

    parse_error(const std::string& msg, std::pair<size_t, size_t> index_span)
        : std::runtime_error(format_message(std::string{}, ErrorInfo{ msg, index_span.first, index_span.second }))
        , err(msg, index_span.first, index_span.second)
    {
    }

    explicit parse_error(const ErrorInfo& err_info)
        : std::runtime_error(format_message(std::string{}, err_info))
        , err(err_info)
    {
    }

    parse_error(const std::string& msg, const ErrorInfo& err_info)
        : std::runtime_error(format_message(msg, err_info))
        , err(err_info)
    {
    }

    parse_error(const std::string& msg, const Element& ele)
        : std::runtime_error(format_message(std::string{}, ErrorInfo{ msg, ele.token.start_idx, ele.token.end_idx }))
        , err(msg, ele.token.start_idx, ele.token.end_idx)
    {
    }

    parse_error(const std::string& msg, const Token& token)
        : std::runtime_error(format_message(std::string{}, ErrorInfo{ msg, token.start_idx, token.end_idx }))
        , err(msg, token.start_idx, token.end_idx)
    {
    }

    virtual ~parse_error()
    {
    }

    inline bool has_error_info() const
    {
        return err.is_err;
    }

    inline const ErrorInfo& get_error() const
    {
        return err;
    }

    inline std::string error_info_to_string(std::string_view jxc_buffer) const
    {
        ErrorInfo e = err;
        e.get_line_and_col_from_buffer(jxc_buffer);
        return e.to_string(jxc_buffer);
    }
};


JXC_END_NAMESPACE(jxc)
