#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
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

    access_log: {
        dest: (stdout)
        #dest: "./test_web_access.log"
    }
    error_log: {
        dest: (stderr)
        #dest: "./test_web_error.log"
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

    location: static_directory {
        path: '/static'
        root: './tools/jxdocgen/static'
    }

    location: {
        path: '/favicon.ico'
        content: path "./tools/jxdocgen/static/favicon.svg"
    }

    location: {
        path: '/logo.svg'
        content: path "./tools/jxdocgen/static/jxc-header-logo.svg"
    }

    location: {
        path: '/invalid'
        response: 500
    }

    location: {
        path: '/styles.css'
        content: inline r"css(
            #header h1 {
                align-self: center;
                margin-left: 2vmin;
            }
            main {
                margin-left: auto;
                margin-right: auto;
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

    location: {
        path: '/'
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
                    <li><a href="/not-found">HTTP 404</a></li>
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


struct StaticDirectoryMount
{
    std::string path;
    std::string root_directory;
    HeaderList headers;

    std::string to_repr() const
    {
        return jxc::format("StaticDirectoryMount(path={}, root_directory={}, headers.size={})",
            jxc::detail::debug_string_repr(path),
            jxc::detail::debug_string_repr(root_directory),
            headers.size());
    }
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


struct Location
{
    std::string path;
    std::string redirect_url;
    Document content;
    HeaderList headers;
    int response = 0;

    inline bool has_response_code() const { return response > 0; }

    std::string to_repr() const
    {
        return jxc::format("Location(path={}, redirect_url={} response={}, content={}, headers.size={})",
            jxc::detail::debug_string_repr(path),
            jxc::detail::debug_string_repr(redirect_url),
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


struct LogTarget
{
    LogDestType type = LogDestType::StandardOut;
    std::string log_file_path;

    void log_message(const std::string& entry)
    {
        switch (type)
        {
        case LogDestType::StandardOut:
            std::cout << entry << "\n";
            break;
        case LogDestType::StandardError:
            std::cerr << entry << "\n";
            break;
        case LogDestType::LogFile:
        {
            std::ofstream fp(log_file_path, std::ios::app);
            fp << entry << "\n";
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
    std::vector<StaticDirectoryMount> mounts;
    std::vector<Location> locations;
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
        for (Location& loc : locations)
        {
            fixup_document(loc.content);
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

        ss << "\tmounts: [\n";
        for (const auto& mount : mounts)
        {
            ss << "\t\t" << mount.to_repr() << "\n";
        }
        ss << "\t]\n";

        ss << "\tlocations: [\n";
        for (const auto& loc : locations)
        {
            ss << "\t\t" << loc.to_repr() << "\n";
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

            const std::string key = std::string(key_ele.token.value.as_view());
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
                jxc::TokenSpan anno = parser.value().annotation;
                if (anno.size() == 0)
                {
                    result.locations.push_back(parse_location());
                }
                else if (anno.size() == 1 && anno[0].value == "static_directory")
                {
                    result.mounts.push_back(parse_static_directory_mount());
                }
                else
                {
                    throw make_parse_error(jxc::format("Invalid location annotation {}", anno.to_repr()));
                }
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

        if (parser.value().type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Expected static file mount to be an Object, got {}", jxc::element_type_to_string(parser.value().type)));
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
                const jxc::ElementType dest_element_type = parser.value().type;
                switch (dest_element_type)
                {
                case jxc::ElementType::BeginExpression:
                {
                    advance_required();
                    if (parser.value().type != jxc::ElementType::ExpressionIdentifier)
                    {
                        throw make_parse_error(jxc::format("Expected ExpressionIdentifier, got {}", jxc::element_type_to_string(parser.value().type)));
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
                    result.type = LogDestType::LogFile;
                    result.log_file_path = parse_string();
                    break;

                default:
                    throw make_parse_error(jxc::format("Expected expression or string, got {}", jxc::element_type_to_string(dest_element_type)));
                    break;
                }
            }
            else
            {
                throw make_parse_error(jxc::format("Invalid log target key {}", jxc::detail::debug_string_repr(key)));
            }
        }

        return result;
    }

    StaticDirectoryMount parse_static_directory_mount()
    {
        StaticDirectoryMount result;

        if (parser.value().type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Expected static file mount to be an Object, got {}", jxc::element_type_to_string(parser.value().type)));
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

            if (key == "path")
            {
                result.path = parse_string();
            }
            else if (key == "root")
            {
                result.root_directory = parse_string();
            }
            else if (key == "headers")
            {
                result.headers = parse_headers();
            }
            else
            {
                throw make_parse_error(jxc::format("Unexpected key {} in StaticDirectoryMount", jxc::detail::debug_string_repr(key)));
            }
        }

        return result;
    }

    Location parse_location()
    {
        const jxc::Element& ele = parser.value();
        if (ele.type != jxc::ElementType::BeginObject)
        {
            throw make_parse_error(jxc::format("Expected location to be an Object, got {}", jxc::element_type_to_string(ele.type)));
        }

        Location result;

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

            if (key == "path")
            {
                result.path = parse_string();
            }
            else if (key == "response")
            {
                result.response = parse_number<int>();
            }
            else if (key == "redirect_url")
            {
                result.redirect_url = parse_string();
            }
            else if (key == "headers")
            {
                result.headers = parse_headers();
            }
            else if (key == "content")
            {
                result.content = parse_document();
            }
            else
            {
                throw make_parse_error(jxc::format("Unexpected key {} in Location", jxc::detail::debug_string_repr(key)));
            }
        }

        return result;
    }

    Document parse_document()
    {
        Document doc;

        if (parser.value().type != jxc::ElementType::String)
        {
            throw make_parse_error(jxc::format("Document requires a string (got {})", jxc::element_type_to_string(parser.value().type)));
        }

        jxc::TokenSpan anno = parser.value().annotation;

        if (anno.size() == 0)
        {
            throw make_parse_error("Document requires annotation `path` or `inline`");
        }

        if (anno.size() != 1 && anno.size() != 4)
        {
            throw make_parse_error(jxc::format("Invalid document annotation {}", anno.to_repr()));
        }

        assert(anno[0].type == jxc::TokenType::Identifier);
        if (anno[0].value == "path")
        {
            const fs::path content_path = fs::path(parse_string());
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
            doc.body = parse_string();

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
    T parse_number()
    {
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

        // Split the number token into its component parts (sign, prefix, value, exponent, suffix)
        //   sign: '', '+', or '-'
        //   prefix: '', '0x', '0b', or '0o'
        //   value: the digits in the number. if the number is floating point, this includes the '.' and the fractional part
        //   exponent: any exponent used, eg. 'e-2' or 'E9'
        //   suffix: any custom numeric suffix used, eg. 'px' or '%'
        jxc::util::NumberTokenSplitResult split_result;
        if (jxc::util::split_number_token_value(tok, split_result, parse_error))
        {
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
            case jxc::TokenType::ObjectKeyIdentifier:
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

    std::string parse_string()
    {
        std::string result;
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
        case jxc::TokenType::ObjectKeyIdentifier:
            // if the token is an identifier, we can just return the value - no need to parse it
            result = std::string(token.value.as_view());
            break;

        default:
            throw make_parse_error(jxc::format("Expected string or identifier, got {}", jxc::token_type_to_string(token.type)));
        }

        return result;
    }
};



std::string read_config_file_data(const fs::path& file_path)
{
    if (fs::exists(file_path))
    {
        std::ifstream fp(file_path, std::ios::in);
        std::ostringstream ss;
        ss << fp.rdbuf();
        return ss.str();
    }
    else
    {
        std::ofstream fp(file_path, std::ios::out);
        fp << s_default_config;
        fp.close();
        return std::string(s_default_config);
    }
}


int main(int argc, const char** argv)
{
    fs::path cfg_file_path = "./web_server_config.jxc";
    if (argc == 2)
    {
        std::string_view arg = argv[1];
        if (arg == "/?" || arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [CONFIG_FILE_PATH]\n";
            std::cout << "\tConfig file path defaults to './web_server_config.jxc'\n";
            std::cout << "\tIf the config file does not exist, the default one will be written to that location.\n";
            return 0;
        }
        else
        {
            cfg_file_path = fs::path(arg);
        }
    }

    WebServerConfigParser parser(read_config_file_data(cfg_file_path));
    WebServerConfig cfg = parser.parse();
    std::cout << cfg.to_repr() << "\n";

    httplib::Server svr;

    svr.set_read_timeout(cfg.read_timeout);
    svr.set_write_timeout(cfg.write_timeout);
    svr.set_idle_interval(cfg.idle_interval);
    svr.set_payload_max_length(cfg.payload_max_length);
    svr.new_task_queue = [&cfg]() { return new httplib::ThreadPool(cfg.worker_threads); };

    svr.set_logger([&cfg](const httplib::Request& req, const httplib::Response& res)
    {
        const bool is_error = res.status >= 400;
        const std::string msg = jxc::format("[{}] HTTP {} Path={} Referer={}, User-Agent={}",
            req.remote_addr,
            res.status,
            jxc::detail::debug_string_repr(req.path),
            jxc::detail::debug_string_repr(req.has_header("Referer") ? req.get_header_value("Referer") : std::string{}),
            jxc::detail::debug_string_repr(req.has_header("User-Agent") ? req.get_header_value("User-Agent") : std::string{}));
        if (is_error)
        {
            cfg.error_log.log_message(msg);
        }
        else
        {
            cfg.access_log.log_message(msg);
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

    for (const StaticDirectoryMount& mount : cfg.mounts)
    {
        if (mount.path.size() > 0)
        {
            svr.set_mount_point(mount.path, mount.root_directory);
        }
        else
        {
            std::cerr << "Warning: static_directory location has no path specified.\n";
        }
    }

    for (const Location& loc : cfg.locations)
    {
        if (loc.redirect_url.size() > 0)
        {
            svr.Get(loc.path, [&loc](const httplib::Request& req, httplib::Response& res)
            {
                for (const auto& pair : loc.headers)
                {
                    res.set_header(pair.first, pair.second);
                }
                res.set_redirect(loc.redirect_url, (loc.response == 0) ? 302 : loc.response);
            });
        }
        else if (loc.content.body.size() > 0)
        {
            svr.Get(loc.path, [&loc](const httplib::Request& req, httplib::Response& res)
            {
                if (loc.has_response_code())
                {
                    res.status = loc.response;
                }
                for (const auto& pair : loc.headers)
                {
                    res.set_header(pair.first, pair.second);
                }
                res.set_content(loc.content.body, loc.content.get_mimetype());
            });
        }
        else if (loc.path.size() > 0)
        {
            svr.Get(loc.path, [&loc](const httplib::Request& req, httplib::Response& res)
            {
                if (loc.has_response_code())
                {
                    res.status = loc.response;
                }
                for (const auto& pair : loc.headers)
                {
                    res.set_header(pair.first, pair.second);
                }
            });
        }
        else
        {
            std::cerr << "Warning: Ignoring location " << loc.to_repr() << ": it has no redirect_url, content, or path\n";
        }
    }

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

