#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include "jxc/jxc.h"
#include "httplib.h"


namespace fs = std::filesystem;


static const char* s_default_config = R"JXC(
{
    listen_host: "localhost"
    listen_port: 8080

    worker_threads: 8
    read_timeout: 5s
    write_timeout: 5s
    idle_interval: 100ms
    payload_max_length: 16mb

    # The logging configuration uses JXC expressions as a format function. The syntax used here is as follows:
    # - Identifiers starting with a `$` are builtin variables ($remote_addr, $status, and $path)
    # - An at symbol indicates that the following token (which can be either an identifer or a string) is the name of a header to insert.
    # - If either a builtin variable or header is prefixed with a `!` token, the value will be quoted and special characters will be escaped.
    access_log: logger{
        dest: stream(stdout)
        #dest: file "./web_server_access.log"
        format: ([$remote_addr] (HTTP $status) Path=!$path Referer=!@"Referer" User-Agent=!@"User-Agent")
    }
    error_log: logger{
        dest: stream(stderr)
        #dest: file "./web_server_error.log"
        format: ([$remote_addr] (HTTP $status) Path=!$path Referer=!@"Referer" User-Agent=!@"User-Agent")
    }

    default_not_found_page: inline r"html(
        <!doctype html>
        <html>
        <head>
            <link rel="stylesheet" href="/static/jxdocgen.css" />
            <link rel="stylesheet" href="/styles.css" />
        </head>
        <body>
        <header id="header">
            <img src="/logo.svg" width="100" height="100" />
            <h1>Web Server Example</h1>
        </header>
        <main>
            <h1>{{STATUS}} Not Found</h1>
            <pre>{{PATH}}</pre>
        </main>
        </body>
        </html>
    )html"

    default_server_error_page: inline r"html(
        <!doctype html>
        <html>
        <head>
            <link rel="stylesheet" href="/static/jxdocgen.css" />
            <link rel="stylesheet" href="/styles.css" />
        </head>
        <body>
        <header id="header">
            <img src="/logo.svg" width="100" height="100" />
            <h1>Web Server Example</h1>
        </header>
        <main>
            <h1>Error {{STATUS}} (Internal Server Error)</h1>
            <pre>{{PATH}}</pre>
        </main>
        </body>
        </html>
    )html"

    #NB. JXC does not require that objects have unique keys.
    # We can take advantage of that here to allow multiple location entries.

    location: static_directory{
        path: '/static'
        root: './tools/jxdocgen/static'
    }

    location: resource{
        path: '/favicon.ico'
        content: path "./tools/jxdocgen/static/favicon.svg"
    }

    location: resource{
        path: '/logo.svg'
        content: path<'image/svg+xml'> "./tools/jxdocgen/static/jxc-header-logo.svg"
    }

    location: {
        path: '/teapot'
        response: http 418
    }

    location: {
        path: '/invalid'
        response: http 500
    }

    location: {
        path: '/styles.css'
        content: inline<'text/css'> r"css(
            body {
                font-family: 'Noto Sans', 'Segoe UI', Helvetica, Tahoma, Geneva, Verdana, sans-serif;
                background-color: #202224;
                color: #e8ecf1;
                margin: 0;
            }
            a {
                color: #009c53;
            }
            #header {
                padding: 1vmin;
                border-bottom: 2px solid #119492;
                display: flex;
                align-content: center;
                background: linear-gradient(90deg, #49bb7e, #2a687c);
            }
            #header h1 {
                margin-left: 2vmin;
                align-self: center;
            }
            main {
                max-width: 80rem;
                margin-left: auto;
                margin-right: auto;
                padding: 2vmin;
                background: #111;
                box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
            }
        )css"
    }

    location: {
        path: '/content.html'
        content: inline r"html(
            <!doctype html>
            <html>
            <head>
                <link rel="stylesheet" href="/static/jxdocgen.css" />
                <link rel="stylesheet" href="/styles.css" />
            </head>
            <body>
            <header id="header">
                <img src="/logo.svg" width="100" height="100" />
                <h1>Web Server Example</h1>
            </header>
            <main>
                <h1>Content Page</h1>
                <p><a href="/">Back to Home</a></p>
            </main>
            </body>
            </html>
        )html"
    }

    location: redirect{
        path: '/old_stuff.html'
        redirect: '/content.html'
    }

    location: {
        path: '/'
        response: http 200
        headers: {
            'X-Test-Header': 'neat'
        }
        content: inline<'text/html'> r"html(
            <!doctype html>
            <html>
            <head>
                <link rel="stylesheet" href="/static/jxdocgen.css" />
                <link rel="stylesheet" href="/styles.css" />
            </head>
            <body>
            <header id="header">
                <img src="/logo.svg" width="100" height="100" />
                <h1>Web Server Example</h1>
            </header>
            <main>
                <h1>Index</h1>
                <p>This is an HTML content page embedded in the web server config file.</p>
                <ul>
                    <li><a href="/content.html">Content Page</a></li>
                    <li><a href="/old_stuff.html">Redirect</a></li>
                    <li><a href="/not-found">HTTP 404</a></li>
                    <li><a href="/teapot">HTTP 418</a></li>
                    <li><a href="/invalid">HTTP 500</a></li>
                </ul>
            </main>
            </body>
            </html>
        )html"
    }

    mime_types: {
        # default mimetype
        null:    'application/octet-stream'

        # by file extension
        html:    'text/html'
        htm:     'text/html'
        shtml:   'text/html'
        css:     'text/css'
        xml:     'text/xml'
        rss:     'text/xml'
        gif:     'image/gif'
        jpg:     'image/jpeg'
        jpeg:    'image/jpeg'
        js:      'application/x-javascript'
        txt:     'text/plain'
        htc:     'text/x-component'
        mml:     'text/mathml'
        png:     'image/png'
        ico:     'image/x-icon'
        jng:     'image/x-jng'
        wbmp:    'image/vnd.wap.wbmp'
        svg:     'image/svg+xml'
        jar:     'application/java-archive'
        pdf:     'application/pdf'
        jardiff: 'application/x-java-archive-diff'
        jnlp:    'application/x-java-jnlp-file'
        run:     'application/x-makeself'
        pl:      'application/x-perl'
        pm:      'application/x-perl'
        rar:     'application/x-rar-compressed'
        rpm:     'application/x-redhat-package-manager'
        tcl:     'application/x-tcl'
        tk:      'application/x-tcl'
        der:     'application/x-x509-ca-cert'
        pem:     'application/x-x509-ca-cert'
        crt:     'application/x-x509-ca-cert'
        xpi:     'application/x-xpinstall'
        zip:     'application/zip'
        deb:     'application/octet-stream'
        bin:     'application/octet-stream'
        exe:     'application/octet-stream'
        dll:     'application/octet-stream'
        dmg:     'application/octet-stream'
        eot:     'application/octet-stream'
        iso:     'application/octet-stream'
        img:     'application/octet-stream'
        msi:     'application/octet-stream'
        msp:     'application/octet-stream'
        msm:     'application/octet-stream'
        mp3:     'audio/mpeg'
        ra:      'audio/x-realaudio'
        mpeg:    'video/mpeg'
        mpg:     'video/mpeg'
        mp4:     'video/mp4'
        mov:     'video/quicktime'
        flv:     'video/x-flv'
        avi:     'video/x-msvideo'
        wmv:     'video/x-ms-wmv'
        asx:     'video/x-ms-asf'
        asf:     'video/x-ms-asf'
        mng:     'video/x-mng'
    }
}
)JXC";


bool read_file_to_string(const fs::path& file_path, std::string& out_string)
{
    if (fs::exists(file_path))
    {
        std::ifstream fp(file_path, std::ios::in);
        std::ostringstream ss;
        ss << fp.rdbuf();
        out_string = ss.str();
        return true;
    }
    return false;
}


// borrowed from https://en.cppreference.com/w/cpp/string/basic_string/replace
size_t string_replace_all(std::string& inout, std::string_view what, std::string_view with)
{
    size_t count = 0;
    for (std::string::size_type pos{}; inout.npos != (pos = inout.find(what.data(), pos, what.length())); pos += with.length(), ++count)
    {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
}


// overly aggressive html entity escape function
std::string html_escape_string(const std::string& str)
{
    std::ostringstream ss;
    for (size_t i = 0; i < str.size(); i++)
    {
        const char ch = str[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
        {
            ss << ch;
            continue;
        }

        switch (ch)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            ss << ch;
            break;
        case '&':
            ss << "&amp;";
            break;
        case '<':
            ss << "&lt;";
            break;
        case '>':
            ss << "&gt;";
            break;
        default:
            // convert all other chars to integer entities
            ss << "&#" << static_cast<int>(ch) << ';';
            break;
        }
    }
    return ss.str();
}


using HeaderList = std::vector<std::pair<std::string, std::string>>;
using TimeDelta = std::chrono::duration<double, std::milli>;
using DataSize = uint64_t;


struct MimeTypeMap
{
    std::string default_mimetype;
    std::unordered_map<std::string, std::string> ext_to_mimetype_map;
};


struct Document
{
    std::string body;
    std::string ext;
    std::string mimetype;

    inline std::string get_mimetype() const
    {
        return (mimetype.size() > 0) ? mimetype : "text/plain";
    }

    // template rendering function (not particularly efficient)
    std::string render_template(std::initializer_list<std::pair<std::string, std::string>> template_vars) const
    {
        std::string result = body;
        for (const auto& pair : template_vars)
        {
            const std::string tmpl_var = std::string("{{") + pair.first + std::string("}}");
            if (body.find(tmpl_var) != std::string::npos)
            {
                const std::string value_escaped = html_escape_string(pair.second);
                string_replace_all(result, tmpl_var, value_escaped);
            }
        }
        return result;
    }

    std::string to_repr() const
    {
        return jxc::format("Document(body.size={}, ext={}, mimetype={})",
            body.size(),
            jxc::detail::debug_string_repr(ext),
            jxc::detail::debug_string_repr(mimetype));
    }
};


struct BaseLocation
{
    std::string path;
    HeaderList headers;

    virtual ~BaseLocation() {}

    virtual std::string to_repr() const = 0;
};


struct StaticDirectoryLocation : public BaseLocation
{
    std::string root_directory;

    virtual ~StaticDirectoryLocation() {}

    std::string to_repr() const override
    {
        return jxc::format("StaticDirectoryLocation(path={}, root_directory={}, headers.size={})",
            jxc::detail::debug_string_repr(path),
            jxc::detail::debug_string_repr(root_directory),
            headers.size());
    }
};


struct RedirectLocation : public BaseLocation
{
    std::string redirect_url;
    int response = 0;

    virtual ~RedirectLocation() {}

    std::string to_repr() const override
    {
        return jxc::format("RedirectLocation(path={}, response={}, redirect_url={}, headers.size={})",
            jxc::detail::debug_string_repr(path),
            response,
            jxc::detail::debug_string_repr(redirect_url),
            headers.size());
    }
};


struct ResourceLocation : public BaseLocation
{
    Document content;
    int response = 0;

    virtual ~ResourceLocation() {}

    inline bool has_response_code() const { return response > 0; }

    std::string to_repr() const override
    {
        return jxc::format("ResourceLocation(path={}, response={}, content={}, headers.size={})",
            jxc::detail::debug_string_repr(path),
            response,
            content.to_repr(),
            headers.size());
    }
};


enum class LogDestType
{
    StandardOut,
    StandardError,
    LogFile,
};


using LogFormatFunc = std::function<std::string(const httplib::Request&, const httplib::Response&)>;


struct LogTarget
{
    LogDestType type = LogDestType::StandardOut;
    LogFormatFunc format_func;
    std::string log_file_path;

    void log_message(const httplib::Request& req, const httplib::Response& res)
    {
        switch (type)
        {
        case LogDestType::StandardOut:
            std::cout << format_func(req, res) << "\n";
            break;
        case LogDestType::StandardError:
            std::cerr << format_func(req, res) << "\n";
            break;
        case LogDestType::LogFile:
        {
            std::ofstream fp(log_file_path, std::ios::app);
            fp << format_func(req, res) << "\n";
            fp.close();
            break;
        }
        default:
            break;
        }
    }

    std::string to_repr() const
    {
        switch (type)
        {
        case LogDestType::StandardOut:
            return "LogTarget(StandardOut)";
        case LogDestType::StandardError:
            return "LogTarget(StandardError)";
        case LogDestType::LogFile:
            return jxc::format("LogTarget(File={})", jxc::detail::debug_string_repr(log_file_path));
        default:
            break;
        }
        return "LogTarget(INVALID)";
    }
};


struct WebServerConfig
{
    std::string listen_host;
    uint16_t listen_port = 8080;
    uint32_t worker_threads = 8;
    TimeDelta read_timeout;
    TimeDelta write_timeout;
    TimeDelta idle_interval;
    DataSize payload_max_length = 1024 * 1024 * 8; // 8MB
    LogTarget access_log;
    LogTarget error_log;
    Document default_not_found_page;
    Document default_server_error_page;
    std::vector<std::shared_ptr<BaseLocation>> locations;
    MimeTypeMap mime_types;

    // call after parsing to fix up the data before using
    void prepare()
    {
        auto fixup_document = [&](Document& doc)
        {
            if (doc.ext.size() > 0 && doc.mimetype.size() == 0 && mime_types.ext_to_mimetype_map.contains(doc.ext))
            {
                doc.mimetype = mime_types.ext_to_mimetype_map[doc.ext];
            }
        };

        fixup_document(default_not_found_page);
        fixup_document(default_server_error_page);

        // for any locations where we have an extension but not a mimetype, lookup the mimetype
        for (auto& loc : locations)
        {
            if (ResourceLocation* res_location = dynamic_cast<ResourceLocation*>(loc.get()))
            {
                fixup_document(res_location->content);
            }
        }
    }

    std::string to_repr() const
    {
        std::ostringstream ss;
        ss << "WebServerConfig{\n";

        ss << "\tlisten_host: " << jxc::detail::debug_string_repr(listen_host) << "\n";
        ss << "\tlisten_port: " << listen_port << "\n";
        ss << "\tworker_threads: " << worker_threads << "\n";
        ss << "\tread_timeout: " << read_timeout << "\n";
        ss << "\twrite_timeout: " << write_timeout << "\n";
        ss << "\tidle_interval: " << idle_interval << "\n";
        ss << "\tpayload_max_length: " << payload_max_length << "\n";

        ss << "\taccess_log: " << access_log.to_repr() << "\n";
        ss << "\terror_log: " << error_log.to_repr() << "\n";

        ss << "\tdefault_not_found_page: " << default_not_found_page.to_repr() << "\n";
        ss << "\tdefault_server_error_page: " << default_server_error_page.to_repr() << "\n";

        ss << "\tlocations: [\n";
        for (const auto& loc : locations)
        {
            ss << "\t\t" << loc->to_repr() << "\n";
        }
        ss << "\t]\n";

        ss << "\tmime_types: {\n";
        ss << "\t\tDEFAULT: " << jxc::detail::debug_string_repr(mime_types.default_mimetype) << ",\n";
        for (const auto& pair : mime_types.ext_to_mimetype_map)
        {
            ss << "\t\t" << jxc::detail::debug_string_repr(pair.first) << ": " << jxc::detail::debug_string_repr(pair.second) << ",\n";
        }
        ss << "\t}\n";

        ss << "}";
        return ss.str();
    }
};


class WebServerConfigParser
{
private:
    std::string jxc_buffer;
    jxc::JumpParser parser;
    jxc::ErrorInfo parse_error;

public:
    WebServerConfigParser(std::string_view jxc_data)
        : jxc_buffer(std::string(jxc_data))
        , parser(jxc_buffer)
    {
    }

    bool has_error() const
    {
        return parse_error.is_err || parser.has_error();
    }

    const jxc::ErrorInfo& get_error() const
    {
        return parse_error.is_err ? parse_error : parser.get_error();
    }

    WebServerConfig parse()
    {
        if (advance())
        {
            return parse_config(parser.value());
        }
        throw std::runtime_error("Failed to parse config");
    }

private:
    std::runtime_error make_parse_error(std::string&& msg)
    {
        if (parse_error.is_err)
        {
            parse_error.get_line_and_col_from_buffer(jxc_buffer);
            return std::runtime_error(jxc::format("ParseError: {} ({})", msg, parse_error.to_string(jxc_buffer)));
        }
        else
        {
            const jxc::Token tok = parser.value().token;
            parse_error = jxc::ErrorInfo(std::move(msg), tok.start_idx, tok.end_idx);
            parse_error.get_line_and_col_from_buffer(jxc_buffer);
            return std::runtime_error(jxc::format("ParseError: {}", parse_error.to_string(jxc_buffer)));
        }
    }

    void require_no_annotation()
    {
        jxc::TokenSpan anno = parser.value().annotation;
        if (anno.size() != 0)
        {
            throw make_parse_error(jxc::format("Unexpected annotation {} (no annotation is valid for this value)", jxc::detail::debug_string_repr(anno.source())));
        }
    }

    std::string_view require_annotation(std::initializer_list<std::string_view> valid_annotations, bool annotation_optional = false)
    {
        // helper for error message formatting
        auto make_valid_anno_list = [&]() -> std::string
        {
            std::ostringstream ss;
            size_t i = 0;
            for (const auto& valid_anno : valid_annotations)
            {
                if (i > 0)
                {
                    ss << ", ";
                }
                ++i;
                ss << jxc::detail::debug_string_repr(valid_anno);
            }
            return ss.str();
        };

        jxc::TokenSpan anno = parser.value().annotation;
        if ((valid_annotations.size() == 0 || annotation_optional) && anno.size() == 0)
        {
            // annotation was optional, and no annotation was specified
            return std::string_view("");
        }
        else if (valid_annotations.size() == 0 && anno.size() > 0)
        {
            // expected no annotation at all, but found one
            throw make_parse_error(jxc::format("Unexpected annotation {}", jxc::detail::debug_string_repr(anno.source())));
        }
        else if (anno.size() != 1)
        {
            // expected single-token annotation
            throw make_parse_error(jxc::format("Invalid annotation {} (valid annotations: {})", jxc::detail::debug_string_repr(anno.source()), make_valid_anno_list()));
        }
        else
        {
            // if the annotation we have is in the list of valid ones, return it
            for (const auto& valid_anno : valid_annotations)
            {
                if (anno[0].value == valid_anno)
                {
                    return anno[0].value.as_view();
                }
            }
            throw make_parse_error(jxc::format("Invalid annotation {} (valid annotations: {})", jxc::detail::debug_string_repr(anno.source()), make_valid_anno_list()));
        }
    }

    bool advance()
    {
        if (parser.next())
        {
            return true;
        }
        else if (parser.get_error().is_err)
        {
            throw std::runtime_error(parser.get_error().to_string(jxc_buffer));
        }
        return false;
    }

    void advance_required()
    {
        if (!advance())
        {
            throw make_parse_error("Unexpected end of stream");
        }
    }

    WebServerConfig parse_config(const jxc::Element& ele)
    {
        if (ele.type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Config root object must be an object (got {})", jxc::element_type_to_string(ele.type)));
        }

        auto result = WebServerConfig{};
        while (advance())
        {
            // we're inside an object, so the next element must be either a key or the end of the object
            const jxc::Element& key_ele = parser.value();
            if (key_ele.type == jxc::ElementType::EndObject)
            {
                break;
            }

            // Take ownership of the key string - FlexParser gives us a view and that view is only valid until we advance.
            const auto key = key_ele.token.value.to_owned();

            advance_required();

            if (key == "listen_host")
            {
                result.listen_host = parse_string();
            }
            else if (key == "listen_port")
            {
                result.listen_port = parse_number<uint16_t>();
            }
            else if (key == "worker_threads")
            {
                result.worker_threads = parse_number<uint32_t>();
            }
            else if (key == "read_timeout")
            {
                result.read_timeout = parse_time_delta();
            }
            else if (key == "write_timeout")
            {
                result.write_timeout = parse_time_delta();
            }
            else if (key == "idle_interval")
            {
                result.idle_interval = parse_time_delta();
            }
            else if (key == "payload_max_length")
            {
                result.payload_max_length = parse_data_size();
            }
            else if (key == "default_not_found_page")
            {
                result.default_not_found_page = parse_document();
            }
            else if (key == "default_server_error_page")
            {
                result.default_server_error_page = parse_document();
            }
            else if (key == "access_log")
            {
                result.access_log = parse_log_target();
            }
            else if (key == "error_log")
            {
                result.error_log = parse_log_target();
            }
            else if (key == "mime_types")
            {
                result.mime_types = parse_mime_types();
            }
            else if (key == "location")
            {
                result.locations.push_back(parse_location());
            }
            else
            {
                throw make_parse_error(jxc::format("Invalid key {}", jxc::detail::debug_string_repr(key)));
            }
        }

        result.prepare();
        return result;
    }

    LogTarget parse_log_target()
    {
        LogTarget result;

        require_annotation({"logger"}, true);

        if (parser.value().type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Expected static file mount to be an Object, got {}", jxc::element_type_to_string(parser.value().type)));
        }

        // log targets must have the annotation "logger"
        if (parser.value().annotation.size() != 1 || parser.value().annotation[0].value != "logger")
        {
            throw make_parse_error(jxc::format("Expected log target to have annotation 'logger', got {}", jxc::detail::debug_string_repr(parser.value().annotation.source())));
        }

        while (advance())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }

            if (parser.value().type != jxc::ElementType::ObjectKey)
            {
                throw make_parse_error(jxc::format("Expected object key, got {}", jxc::element_type_to_string(parser.value().type)));
            }

            const std::string key = parse_token_as_string(parser.value().token);
            advance_required();

            if (key == "dest")
            {
                std::string dest_anno = std::string(require_annotation({ "file", "stream" }, true));

                const jxc::ElementType dest_element_type = parser.value().type;
                switch (dest_element_type)
                {
                case jxc::ElementType::BeginExpression:
                {
                    if (dest_anno != "stream")
                    {
                        throw make_parse_error("`stream` annotation requires expression type");
                    }

                    advance_required();
                    if (parser.value().type != jxc::ElementType::ExpressionToken)
                    {
                        throw make_parse_error(jxc::format("Expected ExpressionToken, got {}", jxc::element_type_to_string(parser.value().type)));
                    }
                    const std::string dest_value = parse_token_as_string(parser.value().token);
                    if (dest_value == "stdout")
                    {
                        result.type = LogDestType::StandardOut;
                    }
                    else if (dest_value == "stderr")
                    {
                        result.type = LogDestType::StandardError;
                    }
                    else
                    {
                        throw make_parse_error(jxc::format("Expected identifier `stdout` or `stderr`, got {}", jxc::detail::debug_string_repr(dest_value)));
                    }
                    advance_required();
                    if (parser.value().type != jxc::ElementType::EndExpression)
                    {
                        throw make_parse_error("Expected end of expression");
                    }
                    break;
                }

                case jxc::ElementType::String:
                    if (dest_anno != "file")
                    {
                        throw make_parse_error("`file` annotation requires string type");
                    }

                    result.type = LogDestType::LogFile;
                    result.log_file_path = parse_string(true);
                    break;

                default:
                    throw make_parse_error(jxc::format("Expected expression or string, got {}", jxc::element_type_to_string(dest_element_type)));
                    break;
                }
            }
            else if (key == "format")
            {
                result.format_func = parse_log_format();
            }
            else
            {
                throw make_parse_error(jxc::format("Invalid log target key {}", jxc::detail::debug_string_repr(key)));
            }
        }

        return result;
    }

    LogFormatFunc parse_log_format()
    {
        if (parser.value().type != jxc::ElementType::BeginExpression)
        {
            throw make_parse_error(jxc::format("Expected log format to be an expression, got {}", jxc::element_type_to_string(parser.value().type)));
        }

        struct LogToken
        {
            enum Type
            {
                TOK_StringLiteral,
                TOK_RemoteAddr,
                TOK_HTTPStatus,
                TOK_Path,
                TOK_Header,
            };
            Type type = TOK_StringLiteral;
            bool use_repr = false;
            std::string value;
        };

        // We're going to compile the expression into a sequence of LogToken objects,
        // which we can use to format the log message in the returned std::function object.
        std::vector<LogToken> format_tokens;

        // Expression parser state
        bool next_token_is_header = false;
        bool next_token_use_repr = false;
        size_t last_end_idx = parser.value().token.end_idx;

        // Helper function to add a token to format_tokens
        auto push_token = [&](size_t value_start_idx, LogToken::Type tok_type, const std::string& value)
        {
            // find and insert any whitespace between the last token and this one
            if (value_start_idx > last_end_idx)
            {
                std::vector<char> sep;
                std::string_view view = parser.get_buffer().substr(last_end_idx, value_start_idx - last_end_idx);
                for (size_t i = 0; i < view.size(); i++)
                {
                    const char ch = view[i];
                    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                    {
                        sep.push_back(ch);
                    }
                }
                if (sep.size() > 0)
                {
                    format_tokens.push_back(LogToken{ LogToken::TOK_StringLiteral, false, std::string(sep.begin(), sep.end()) });
                }
            }

            // add the token
            format_tokens.push_back(LogToken{ tok_type, next_token_use_repr, value });
            next_token_use_repr = false;

            // remember where this last token ended so we can add whitespace between this token and the next one
            last_end_idx = parser.value().token.end_idx;
        };

        while (advance())
        {
            if (parser.value().type == jxc::ElementType::EndExpression)
            {
                break;
            }

            const jxc::Token& tok = parser.value().token;

            if (next_token_is_header)
            {
                // if the next token must be a header, require an identifier or string token
                std::string header_name;
                switch (tok.type)
                {
                case jxc::TokenType::Identifier:
                    header_name = std::string(tok.value.as_view());
                    break;
                case jxc::TokenType::String:
                    if (!jxc::util::parse_string_token<std::string>(tok, header_name, parse_error))
                    {
                        throw make_parse_error("Invalid string token");
                    }
                    break;
                default:
                    throw make_parse_error(jxc::format("Expected token following @ to be an identifier or string, got {}", jxc::token_type_to_string(tok.type)));
                }

                push_token(tok.start_idx, LogToken::TOK_Header, header_name);
                next_token_is_header = false;
            }
            else
            {
                switch (tok.type)
                {
                // `@` symbol indicates that the next token (string or identifier) is a header name
                case jxc::TokenType::AtSymbol:
                    next_token_is_header = true;
                    break;

                // `!` symbol indicates that if the next token is a string, it should be quoted and escaped (similar to the Python repr() function)
                case jxc::TokenType::ExclamationPoint:
                    next_token_use_repr = true;
                    break;

                // Identifiers can either be a builtin variable (if they start with `$`), or treated as a string literal
                case jxc::TokenType::Identifier:
                    if (tok.value.size() > 0 && tok.value[0] == '$')
                    {
                        // identifier starts with a '$' symbol, so treat it as a builtin variable
                        std::string_view var_name = tok.value.as_view().substr(1);
                        if (var_name == "remote_addr")
                        {
                            push_token(tok.start_idx, LogToken::TOK_RemoteAddr, std::string{});
                        }
                        else if (var_name == "status")
                        {
                            push_token(tok.start_idx, LogToken::TOK_HTTPStatus, std::string{});
                        }
                        else if (var_name == "path")
                        {
                            push_token(tok.start_idx, LogToken::TOK_Path, std::string{});
                        }
                        else
                        {
                            throw make_parse_error(jxc::format("Invalid log format variable name {}", jxc::detail::debug_string_repr(var_name)));
                        }
                    }
                    else
                    {
                        // treat this identifier as a string literal
                        push_token(tok.start_idx, LogToken::TOK_StringLiteral, std::string(tok.value.as_view()));
                    }
                    break;

                // String literals that are _not_ prefixed with `@` should be inserted into the format string directly
                case jxc::TokenType::String:
                {
                    std::string str_value;
                    if (!jxc::util::parse_string_token<std::string>(tok, str_value, parse_error))
                    {
                        throw make_parse_error("Invalid string token");
                    }
                    push_token(tok.start_idx, LogToken::TOK_StringLiteral, str_value);
                    break;
                }

                // Any other tokens in the expression should be added to the format string directly as a string literal
                default:
                    push_token(tok.start_idx, LogToken::TOK_StringLiteral, std::string(tok.value.as_view()));
                    break;
                }
            }
        }

        // Create a std::function object that contains a copy of our format_tokens vector, which is used to format the resulting log entry
        return [format_tokens](const httplib::Request& req, const httplib::Response& res) -> std::string
        {
            std::ostringstream ss;
            for (const LogToken& tok : format_tokens)
            {
                switch (tok.type)
                {
                case LogToken::TOK_StringLiteral:
                    ss << (tok.use_repr ? jxc::detail::debug_string_repr(tok.value) : tok.value);
                    break;
                case LogToken::TOK_RemoteAddr:
                    ss << (tok.use_repr ? jxc::detail::debug_string_repr(req.remote_addr) : req.remote_addr);
                    break;
                case LogToken::TOK_HTTPStatus:
                    ss << res.status;
                    break;
                case LogToken::TOK_Path:
                    ss << (tok.use_repr ? jxc::detail::debug_string_repr(req.path) : req.path);
                    break;
                case LogToken::TOK_Header:
                    if (req.has_header(tok.value))
                    {
                        ss << (tok.use_repr ? jxc::detail::debug_string_repr(req.get_header_value(tok.value)) : req.get_header_value(tok.value));
                    }
                    else if (res.has_header(tok.value))
                    {
                        ss << (tok.use_repr ? jxc::detail::debug_string_repr(res.get_header_value(tok.value)) : res.get_header_value(tok.value));
                    }
                    else
                    {
                        ss << (tok.use_repr ? jxc::detail::debug_string_repr("None") : "None");
                    }
                    break;
                default:
                    throw std::runtime_error("Invalid log token type");
                }
            }
            return ss.str();
        };
    }

    std::shared_ptr<BaseLocation> parse_location()
    {
        std::string loc_anno = std::string(require_annotation({ "resource", "redirect", "static_directory" }, true));

        // default to resource if no annotation specified
        if (loc_anno.size() == 0)
        {
            loc_anno = "resource";
        }

        std::shared_ptr<BaseLocation> location;
        if (loc_anno == "resource")
        {
            location = std::shared_ptr<BaseLocation>(new ResourceLocation());
        }
        else if (loc_anno == "redirect")
        {
            location = std::shared_ptr<BaseLocation>(new RedirectLocation());
        }
        else if (loc_anno == "static_directory")
        {
            location = std::shared_ptr<BaseLocation>(new StaticDirectoryLocation());
        }
        else
        {
            assert(false && "require_annotation should make this an unreachable branch");
        }

        if (parser.value().type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Expected location to be an Object, got {}", jxc::element_type_to_string(parser.value().type)));
        }

        while (advance())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }
            
            if (parser.value().type != jxc::ElementType::ObjectKey)
            {
                throw make_parse_error(jxc::format("Expected object key, got {}", jxc::element_type_to_string(parser.value().type)));
            }

            const std::string key = parse_token_as_string(parser.value().token);
            advance_required();

            bool key_unhandled = false;

            // base class props
            if (key == "path")
            {
                location->path = parse_string();
            }
            else if (key == "headers")
            {
                location->headers = parse_headers();
            }
            else
            {
                // child class props
                if (ResourceLocation* res_location = dynamic_cast<ResourceLocation*>(location.get()))
                {
                    if (key == "response")
                    {
                        require_annotation({ "http" });
                        res_location->response = parse_number<int>(true);
                    }
                    else if (key == "content")
                    {
                        res_location->content = parse_document();
                    }
                    else
                    {
                        key_unhandled = true;
                    }
                }
                else if (RedirectLocation* redir_location = dynamic_cast<RedirectLocation*>(location.get()))
                {
                    if (key == "redirect")
                    {
                        redir_location->redirect_url = parse_string();
                    }
                    else if (key == "response")
                    {
                        require_annotation({ "http" });
                        redir_location->response = parse_number<int>(true);
                    }
                    else
                    {
                        key_unhandled = true;
                    }
                }
                else if (StaticDirectoryLocation* static_location = dynamic_cast<StaticDirectoryLocation*>(location.get()))
                {
                    if (key == "root")
                    {
                        static_location->root_directory = parse_string();
                    }
                    else
                    {
                        key_unhandled = true;
                    }
                }
                else
                {
                    throw std::runtime_error("Unhandled location subclass");
                }
            }

            if (key_unhandled)
            {
                throw make_parse_error(jxc::format("Unexpected key {} in {} location", jxc::detail::debug_string_repr(key), loc_anno));
            }
        }

        return location;
    }

    Document parse_document()
    {
        Document doc;

        if (parser.value().type != jxc::ElementType::String)
        {
            throw make_parse_error(jxc::format("Document requires a string (got {})", jxc::element_type_to_string(parser.value().type)));
        }

        // custom annotation handling here, to support allowing a mimetype to be specified as the annotation's "generic" argument
        jxc::TokenSpan anno = parser.value().annotation;
        if (anno.size() == 0)
        {
            throw make_parse_error("Document requires annotation `path` or `inline`");
        }
        else if (anno.size() != 1 && anno.size() != 4)
        {
            throw make_parse_error(jxc::format("Invalid document annotation {}", anno.to_repr()));
        }

        assert(anno[0].type == jxc::TokenType::Identifier);
        if (anno[0].value == "path")
        {
            const fs::path content_path = fs::path(parse_string(true));
            if (!read_file_to_string(content_path, doc.body))
            {
                throw make_parse_error(jxc::format("Failed to read content from path {}", jxc::detail::debug_string_repr(content_path.string())));
            }
            doc.ext = content_path.extension().string();
            if (doc.ext.size() > 0 && doc.ext[0] == '.')
            {
                doc.ext = doc.ext.substr(1);
            }
        }
        else if (anno[0].value == "inline")
        {
            doc.body = parse_string(true);

            // If content is a raw string, we can access its heredoc using the string token's tag.
            // Assume the heredoc is a file extension so we can look up the mimetype later if needed.
            if (parser.value().token.type == jxc::TokenType::String && parser.value().token.tag.size() > 0)
            {
                doc.ext = std::string(parser.value().token.tag.as_view());
            }
        }

        // allow specifying mimetype in annotation (eg. inline<'text/plain'>)
        if (anno.size() == 4)
        {
            if (anno[1].type != jxc::TokenType::AngleBracketOpen || anno[3].type != jxc::TokenType::AngleBracketClose)
            {
                throw make_parse_error("Invalid document annotation - expected angle brackets");
            }
            doc.mimetype = parse_token_as_string(anno[2]);
        }

        return doc;
    }

    HeaderList parse_headers()
    {
        HeaderList result;
        require_no_annotation();

        if (parser.value().type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Headers requires an object (got {})", jxc::element_type_to_string(parser.value().type)));
        }
        while (advance())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }
            std::string key = parse_token_as_string(parser.value().token);
            advance_required();
            result.push_back(std::make_pair(std::move(key), parse_token_as_string(parser.value().token)));
        }
        return result;
    }

    template<typename T>
    T parse_number(bool allow_annotation = false)
    {
        if (!allow_annotation)
        {
            require_no_annotation();
        }

        const jxc::Token& tok = parser.value().token;
        if (tok.type != jxc::TokenType::Number)
        {
            throw make_parse_error(jxc::format("Expected Number token, got {}", jxc::token_type_to_string(tok.type)));
        }

        T result = static_cast<T>(0);
        std::string err;
        std::string suffix;
        if (!jxc::util::parse_number_simple<T>(tok.value.as_view(), result, &err, &suffix))
        {
            throw make_parse_error(jxc::format("Failed parsing number: {}", err));
        }

        // don't allow any numeric suffix
        if (suffix.size() > 0)
        {
            throw make_parse_error(jxc::format("Unexpected numeric suffix {}", jxc::detail::debug_string_repr(suffix)));
        }
        return result;
    }

    template<typename T>
    T parse_number_with_suffix(std::initializer_list<std::string> allowed_suffixes, std::string& out_suffix)
    {
        const jxc::Token& tok = parser.value().token;
        if (tok.type != jxc::TokenType::Number)
        {
            throw make_parse_error(jxc::format("Expected Number token, got {}", jxc::token_type_to_string(tok.type)));
        }

        // Split the number token into its component parts (sign, prefix, value, exponent, suffix, float_type)
        //   sign: '', '+', or '-'
        //   prefix: '', '0x', '0b', or '0o'
        //   value: the digits in the number. if the number is floating point, this includes the '.' and the fractional part
        //   exponent: any exponent used, eg. 'e-2' or 'E9'
        //   suffix: any custom numeric suffix used, eg. 'px' or '%'
        jxc::util::NumberTokenSplitResult split_result;
        if (jxc::util::split_number_token_value(tok, split_result, parse_error))
        {
            if constexpr (std::is_integral_v<T>)
            {
                if (split_result.is_floating_point())
                {
                    throw make_parse_error("Expected integer, got floating point value");
                }
            }

            // if a list of allowed numeric suffixes were passed in, verify that this suffix is in that list
            if (allowed_suffixes.size() > 0)
            {
                bool suffix_is_valid = false;
                for (const auto& suffix : allowed_suffixes)
                {
                    if (split_result.suffix == suffix)
                    {
                        suffix_is_valid = true;
                        break;
                    }
                }

                if (!suffix_is_valid)
                {
                    std::ostringstream ss;
                    ss << "Invalid numeric suffix " << jxc::detail::debug_string_repr(split_result.suffix);
                    ss << " (allowed values: ";
                    for (const auto& suffix : allowed_suffixes)
                    {
                        ss << jxc::detail::debug_string_repr(suffix) << ", ";
                    }
                    ss << ")";
                    throw make_parse_error(ss.str());
                }
            }

            // return the suffix used
            out_suffix = std::string(split_result.suffix);

            // parse the string to a valid number of the requested type
            T result = static_cast<T>(0);
            if (jxc::util::parse_number<T>(tok, result, split_result, parse_error))
            {
                return result;
            }
        }

        throw make_parse_error("Failed to parse number");
    }

    TimeDelta parse_time_delta()
    {
        require_no_annotation();
        std::string suffix;
        const double value = parse_number_with_suffix<double>({ "s", "ms" }, suffix);
        if (suffix == "s")
        {
            return TimeDelta(value * 1000.0);
        }
        else if (suffix == "ms")
        {
            return TimeDelta(value);
        }
        assert(false && "invalid suffix");
        return TimeDelta(0.0);
    }

    DataSize parse_data_size()
    {
        require_no_annotation();
        std::string suffix;
        const double value = parse_number_with_suffix<double>({ "b", "kb", "mb", "gb", "tb" }, suffix);
        if (suffix == "b")
        {
            return static_cast<DataSize>(value);
        }
        else if (suffix == "kb")
        {
            return static_cast<DataSize>(value * 1024);
        }
        else if (suffix == "mb")
        {
            return static_cast<DataSize>(value * 1024 * 1024);
        }
        else if (suffix == "gb")
        {
            return static_cast<DataSize>(value * 1024 * 1024 * 1024);
        }
        else if (suffix == "tb")
        {
            return static_cast<DataSize>(value * 1024 * 1024 * 1024 * 1024);
        }
        assert(false && "invalid suffix");
        return 0;
    }

    MimeTypeMap parse_mime_types()
    {
        MimeTypeMap result;

        require_no_annotation();

        if (parser.value().type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Expected mime types to be an object, got {}", jxc::element_type_to_string(parser.value().type)));
        }

        while (advance())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }

            const jxc::Element& key_ele = parser.value();
            assert(key_ele.type == jxc::ElementType::ObjectKey);

            std::string mimetype_ext;
            switch (key_ele.token.type)
            {
            case jxc::TokenType::Null:
                // allow null key - means the default mimetype
                break;
            case jxc::TokenType::Identifier:
            case jxc::TokenType::String:
                mimetype_ext = parse_token_as_string(key_ele.token);
                break;
            default:
                throw make_parse_error(jxc::format("Expected null, string, or identifer for mimetype key, got {}", jxc::token_type_to_string(key_ele.token.type)));
            }

            // advance to mimetype value
            advance_required();

            if (mimetype_ext.size() == 0)
            {
                result.default_mimetype = parse_token_as_string(parser.value().token);
            }
            else
            {
                result.ext_to_mimetype_map.insert_or_assign(mimetype_ext, parse_token_as_string(parser.value().token));
            }
        }

        return result;
    }

    std::string parse_string(bool allow_annotation = false)
    {
        std::string result;

        if (!allow_annotation)
        {
            require_no_annotation();
        }

        if (parser.value().type == jxc::ElementType::String)
        {
            if (!jxc::util::parse_string_token(parser.value().token, result, parse_error))
            {
                throw make_parse_error("Failed to parse string");
            }
        }
        return result;
    }

    std::string parse_token_as_string(const jxc::Token& token)
    {
        std::string result;

        switch (token.type)
        {
        case jxc::TokenType::String:
            // if the token is a string, parse it first (this handles string escapes, raw strings, heredocs, etc.)
            if (!jxc::util::parse_string_token<std::string>(token, result, parse_error))
            {
                throw make_parse_error("Failed to parse string");
            }
            break;

        case jxc::TokenType::Identifier:
            // if the token is an identifier, we can just return the value - no need to parse it
            result = std::string(token.value.as_view());
            break;

        default:
            throw make_parse_error(jxc::format("Expected string or identifier, got {}", jxc::token_type_to_string(token.type)));
        }

        return result;
    }
};


struct ParsedArgs
{
    fs::path config_path = "./web_server_config.jxc";
    bool config_debug = false;

    static void print_usage(int argc, const char** argv)
    {
        ParsedArgs default_args{};
        std::cout << "Usage: [--help] [--debug] [--config path/to/config.jxc]\n";
        std::cout << "\t--help: Show this help message\n";
        std::cout << "\t--debug: Dump parsed config file data for debugging\n";
        std::cout << "\t--config: Specify path to config file (default=" << jxc::detail::debug_string_repr(default_args.config_path.string()) << "\n";
        std::cout << "\t\tIf the config file does not exist, the default one will be written to that location.\n";
    }

    static ParsedArgs parse(int argc, const char** argv)
    {
        ParsedArgs args;

        static const std::string config_arg_long = "--config";
        static const std::string config_arg_long_eq = config_arg_long + "=";
        static const std::string config_arg_short = config_arg_long.substr(1, 2);

        bool expecting_next_arg_config_path = false;
        size_t i = 1;
        while (i < argc)
        {
            std::string_view arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?")
            {
                print_usage(argc, argv);
                std::exit(0);
            }
            else if (arg == "--debug" || arg == "-d")
            {
                args.config_debug = true;
            }
            else if (arg.starts_with(config_arg_long_eq))
            {
                std::string_view arg_value = arg.substr(config_arg_long_eq.size());
                args.config_path = fs::path(arg_value);
            }
            else if (expecting_next_arg_config_path)
            {
                expecting_next_arg_config_path = false;
                args.config_path = fs::path(arg);
            }
            else if (arg == config_arg_long || arg == config_arg_short)
            {
                expecting_next_arg_config_path = true;
            }
            else
            {
                std::cerr << "Invalid argument " << jxc::detail::debug_string_repr(arg) << ": see --help for usage.\n";
                std::exit(1);
            }

            ++i;
        }

        if (expecting_next_arg_config_path)
        {
            std::cerr << "Missing config file path: see --help for usage.\n";
            std::exit(1);
        }

        return args;
    }
};


std::string read_config_file_data(const fs::path& file_path, bool debug_mode)
{
    if (fs::exists(file_path))
    {
        if (debug_mode)
        {
            std::cout << "Reading existing config file " << jxc::detail::debug_string_repr(file_path.string()) << "\n";
        }

        std::ifstream fp(file_path, std::ios::in);
        std::ostringstream ss;
        ss << fp.rdbuf();
        return ss.str();
    }
    else
    {
        if (debug_mode)
        {
            std::cout << "Config file " << jxc::detail::debug_string_repr(file_path.string()) << " does not exist; creating it.\n";
        }

        std::ofstream fp(file_path, std::ios::out);
        fp << s_default_config;
        fp.close();
        return std::string(s_default_config);
    }
}


int main(int argc, const char** argv)
{
    auto args = ParsedArgs::parse(argc, argv);

    // read or create the config file
    const std::string config_file_data = read_config_file_data(args.config_path, args.config_debug);
    WebServerConfigParser parser(config_file_data);

    // parse the config file
    WebServerConfig cfg = parser.parse();
    if (args.config_debug)
    {
        std::cout << cfg.to_repr() << "\n";
    }

    // set up the web server
    httplib::Server svr;
    svr.set_read_timeout(cfg.read_timeout);
    svr.set_write_timeout(cfg.write_timeout);
    svr.set_idle_interval(cfg.idle_interval);
    svr.set_payload_max_length(cfg.payload_max_length);
    svr.new_task_queue = [&cfg]() { return new httplib::ThreadPool(cfg.worker_threads); };

    svr.set_logger([&cfg](const httplib::Request& req, const httplib::Response& res)
    {
        const bool is_error = res.status >= 400;
        if (is_error)
        {
            cfg.error_log.log_message(req, res);
        }
        else
        {
            cfg.access_log.log_message(req, res);
        }
    });

    for (const auto& pair : cfg.mime_types.ext_to_mimetype_map)
    {
        svr.set_file_extension_and_mimetype_mapping(pair.first, pair.second);
    }

    svr.set_error_handler([&cfg](const httplib::Request& req, httplib::Response& res)
    {
        if (res.body.size() == 0 && res.status >= 400)
        {
            const Document& doc = (res.status == 404) ? cfg.default_not_found_page : cfg.default_server_error_page;
            res.set_content(doc.render_template({
                    { "PATH", req.path },
                    { "STATUS", std::to_string(res.status) },
                }),
                doc.get_mimetype());
        }
    });

    for (const auto& loc : cfg.locations)
    {
        if (ResourceLocation* res_loc = dynamic_cast<ResourceLocation*>(loc.get()))
        {
            if (res_loc->content.body.size() > 0)
            {
                svr.Get(loc->path, [loc](const httplib::Request& req, httplib::Response& res)
                {
                    for (const auto& pair : loc->headers)
                    {
                        res.set_header(pair.first, pair.second);
                    }

                    ResourceLocation& resource = *dynamic_cast<ResourceLocation*>(loc.get());
                    if (resource.has_response_code())
                    {
                        res.status = resource.response;
                    }
                    res.set_content(resource.content.body, resource.content.get_mimetype());
                });
            }
            else if (loc->path.size() > 0)
            {
                // no document, but this is still a valid path - just don't return a body
                svr.Get(loc->path, [loc](const httplib::Request& req, httplib::Response& res)
                {
                    for (const auto& pair : loc->headers)
                    {
                        res.set_header(pair.first, pair.second);
                    }

                    ResourceLocation& resource = *dynamic_cast<ResourceLocation*>(loc.get());
                    if (resource.has_response_code())
                    {
                        res.status = resource.response;
                    }
                });
            }
        }
        else if (dynamic_cast<RedirectLocation*>(loc.get()) != nullptr)
        {
            svr.Get(loc->path, [loc](const httplib::Request& req, httplib::Response& res)
            {
                for (const auto& pair : loc->headers)
                {
                    res.set_header(pair.first, pair.second);
                }

                RedirectLocation& redir = *dynamic_cast<RedirectLocation*>(loc.get());
                res.set_redirect(redir.redirect_url, (redir.response == 0) ? 302 : redir.response);
            });
        }
        else if (StaticDirectoryLocation* static_loc = dynamic_cast<StaticDirectoryLocation*>(loc.get()))
        {
            // make sure the static directory exists and is valid
            if (!fs::exists(static_loc->root_directory) || !fs::is_directory(static_loc->root_directory))
            {
                throw std::runtime_error(jxc::format("Static location with root directory {} (path={}) does not exist or is not a directory",
                    jxc::detail::debug_string_repr(static_loc->root_directory),
                    jxc::detail::debug_string_repr(static_loc->path)));
            }
            else if (static_loc->path.size() == 0)
            {
                throw std::runtime_error(jxc::format("Static location with root directory {} does not have a path specified",
                    jxc::detail::debug_string_repr(static_loc->root_directory)));
            }

            httplib::Headers header_dict;
            for (const auto& pair : loc->headers)
            {
                header_dict.insert({ pair.first, pair.second });
            }
            svr.set_mount_point(static_loc->path, static_loc->root_directory, header_dict);
        }
        else
        {
            std::cerr << "Warning: Ignoring invalid location " << loc->to_repr() << "\n";
        }
    }

    // start the web server
    std::cout << "Web server started; listening on " << cfg.listen_host << ":" << cfg.listen_port << std::endl;
    if (cfg.listen_port == 0)
    {
        svr.bind_to_any_port(cfg.listen_host);
    }
    else
    {
        svr.bind_to_port(cfg.listen_host, cfg.listen_port);
    }

    if (!svr.listen_after_bind())
    {
        std::cerr << "HTTP server failed to start listening on " << cfg.listen_host << ":" << cfg.listen_port << std::endl;
        return 1;
    }

    return 0;
}

