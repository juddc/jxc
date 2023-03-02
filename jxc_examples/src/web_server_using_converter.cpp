//
// This example uses the jxc_cpp library's struct auto-conversion features to
// remove most of the boilerplate involved in parsing simple C/C++ structs.
//
// See also web_server_using_second_stage_parser.cpp for an example of how to
// use the lower level parsing API for much greater flexibility if that is needed.
//
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
#include "jxc_cpp/jxc_cpp.h"
#include "thirdparty/httplib.h"


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


static bool read_file_to_string(const fs::path& file_path, std::string& out_string)
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
static size_t string_replace_all(std::string& inout, std::string_view what, std::string_view with)
{
    size_t count = 0;
    for (std::string::size_type pos{}; inout.npos != (pos = inout.find(what.data(), pos, what.length())); pos += with.length(), ++count)
    {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
}


// overly aggressive html entity escape function
static std::string html_escape_string(const std::string& str)
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


// HTTP headers can be specified multiple times, so we can't use an unordered_map here
using HeaderList = std::vector<std::pair<std::string, std::string>>;

using TimeDelta = std::chrono::duration<double, std::milli>;
using DataSizeBytes = uint64_t;


inline std::optional<DataSizeBytes> make_data_size_from_suffix(uint64_t value, std::string_view suffix)
{
    if (suffix == "b")
    {
        return static_cast<DataSizeBytes>(value);
    }
    else if (suffix == "kb")
    {
        return static_cast<DataSizeBytes>(value * 1024);
    }
    else if (suffix == "mb")
    {
        return static_cast<DataSizeBytes>(value * 1024 * 1024);
    }
    else if (suffix == "gb")
    {
        return static_cast<DataSizeBytes>(value * 1024 * 1024 * 1024);
    }
    else if (suffix == "tb")
    {
        return static_cast<DataSizeBytes>(value * 1024 * 1024 * 1024 * 1024);
    }
    else
    {
        return std::nullopt;
    }
}


// Map of file extensions to mimetypes
struct MimeTypeMap
{
    std::string default_mimetype;
    std::unordered_map<std::string, std::string> ext_to_mimetype_map;
};


// Text document (html/css/etc.) loaded in memory that can be sent as an HTTP response
struct TextDocument
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
        return jxc::format("TextDocument(body.size={}, ext={}, mimetype={})",
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
    TextDocument content;
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


// Converter implementations


template<>
struct jxc::Converter<TimeDelta>
{
    using value_type = TimeDelta;

    static const jxc::TokenList& get_annotation()
    {
        static const jxc::TokenList anno = jxc::TokenList::from_identifier("timedelta");
        return anno;
    }

    static value_type parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        parser.require_no_annotation();
        std::string suffix;
        const double value = parser.parse_token_as_number<double>(parser.value().token, { "s", "ms" }, suffix);
        if (suffix == "s")
        {
            return TimeDelta(value * 1000.0);
        }
        else if (suffix == "ms")
        {
            return TimeDelta(value);
        }
        JXC_UNREACHABLE("Invalid suffix {}", suffix);
    }
};


template<>
struct jxc::Converter<HeaderList>
{
    using value_type = HeaderList;

    static const jxc::TokenList& get_annotation()
    {
        static const jxc::TokenList anno = jxc::TokenList::from_identifier("HeaderList");
        return anno;
    }

    static HeaderList parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        HeaderList result;
        parser.require_no_annotation();
        parser.require(jxc::ElementType::BeginObject);
        jxc::conv::parse_map<std::string, std::string>(parser, "HeaderList", jxc::TokenView(), jxc::TokenView(),
            [&result](std::string&& key, std::string&& value)
            {
                result.push_back(std::make_pair(std::move(key), std::move(value)));
            });
        return result;
    }
};


template<>
struct jxc::Converter<MimeTypeMap>
{
    using value_type = MimeTypeMap;

    static const jxc::TokenList& get_annotation()
    {
        static const jxc::TokenList anno = jxc::TokenList::from_identifier("mimetypes");
        return anno;
    }

    static value_type parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        MimeTypeMap result;

        if (TokenView anno = parser.get_value_annotation(generic_anno))
        {
            throw jxc::parse_error("Unexpected annotation", anno.get_index_span());
        }

        parser.require(jxc::ElementType::BeginObject);

        while (parser.next())
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
                mimetype_ext = parser.parse_token_as_string(key_ele.token);
                break;
            default:
                throw jxc::parse_error(jxc::format("Expected null, string, or identifer for mimetype key, got {}", jxc::token_type_to_string(key_ele.token.type)),
                    key_ele);
            }

            // advance to mimetype value
            parser.require_next();

            if (mimetype_ext.size() == 0)
            {
                result.default_mimetype = parser.parse_token_as_string(parser.value().token);
            }
            else
            {
                result.ext_to_mimetype_map.insert_or_assign(mimetype_ext, parser.parse_token_as_string(parser.value().token));
            }
        }

        return result;
    }
};


template<>
struct jxc::Converter<TextDocument>
{
    using value_type = TextDocument;

    static const jxc::TokenList& get_annotation()
    {
        static const jxc::TokenList anno = jxc::TokenList::from_identifier("document");
        return anno;
    }

    static value_type parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        value_type doc;

        parser.require(jxc::ElementType::String);

        bool is_path = false;
        bool is_inline = false;

        // custom annotation handling here, to support allowing a mimetype to be specified as the annotation's "generic" argument
        jxc::TokenView anno = parser.get_value_annotation(generic_anno);
        if (!anno)
        {
            throw jxc::parse_error("TextDocument requires annotation `path` or `inline`");
        }
        else
        {
            jxc::AnnotationParser anno_parser = parser.make_annotation_parser(anno);
            if (anno_parser.equals(jxc::TokenType::Identifier, "path"))
            {
                is_path = true;
            }
            else if (anno_parser.equals(jxc::TokenType::Identifier, "inline"))
            {
                is_inline = true;
            }
            else
            {
                throw jxc::parse_error(jxc::format("Expected annotation 'path' or 'inline', got {}", anno_parser.current().value.as_view()), anno_parser.current());
            }

            // allow specifying mimetype in annotation (eg. inline<'text/plain'>)
            if (anno_parser.advance())
            {
                anno_parser.require(jxc::TokenType::AngleBracketOpen);
                TokenView doc_generic = anno_parser.skip_over_generic_value();
                anno_parser.require_then_advance(jxc::TokenType::AngleBracketClose);
                anno_parser.done_required();

                if (doc_generic.size() == 1 && doc_generic[0].type == jxc::TokenType::String)
                {
                    doc.mimetype = parser.parse_token_as_string(doc_generic[0]);
                }
                else if (doc_generic.size() > 0)
                {
                    throw jxc::parse_error(jxc::format("Expected mimetype string inside angle brackets: {}", doc_generic.to_repr()),
                        doc_generic[0].start_idx, doc_generic[doc_generic.size() - 1].end_idx);
                }
            }
        }

        if (is_path)
        {
            const fs::path content_path = fs::path(parser.parse_token_as_string(parser.value().token));
            if (!read_file_to_string(content_path, doc.body))
            {
                throw jxc::parse_error(jxc::format("Failed to read content from path {}", jxc::detail::debug_string_repr(content_path.string())));
            }
            doc.ext = content_path.extension().string();
            if (doc.ext.size() > 0 && doc.ext[0] == '.')
            {
                doc.ext = doc.ext.substr(1);
            }
        }
        else if (is_inline)
        {
            doc.body = parser.parse_token_as_string(parser.value().token);

            // If content is a raw string, we can access its heredoc using the string token's tag.
            // Assume the heredoc is a file extension so we can look up the mimetype later if needed.
            if (parser.value().token.type == jxc::TokenType::String && parser.value().token.tag.size() > 0)
            {
                doc.ext = std::string(parser.value().token.tag.as_view());
            }
        }
        else
        {
            JXC_ASSERTF(false, "Invalid document type");
        }

        return doc;
    }
};


template<>
struct jxc::Converter<std::shared_ptr<BaseLocation>>
{
    using value_type = std::shared_ptr<BaseLocation>;

    static value_type parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        std::shared_ptr<BaseLocation> location;

        const bool annotation_is_optional = true;
        const std::string loc_anno = std::string(parser.require_identifier_annotation(
            parser.get_value_annotation(generic_anno),
            { "resource", "redirect", "static_directory" },
            annotation_is_optional));

        // default to resource if no annotation specified
        if (loc_anno == "resource" || loc_anno.size() == 0)
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

        parser.require(jxc::ElementType::BeginObject);
        while (parser.next())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }

            // get the next object key, then advance to the value
            parser.require(jxc::ElementType::ObjectKey);
            const std::string key = parser.parse_token_as_string(parser.value().token);
            parser.require_next();

            bool key_unhandled = false;

            // base class props
            if (key == "path")
            {
                parser.require_no_annotation();
                location->path = parser.parse_token_as_string(parser.value().token);
            }
            else if (key == "headers")
            {
                location->headers = parser.parse_value<HeaderList>();
            }
            else
            {
                // child class props
                if (ResourceLocation* res_location = dynamic_cast<ResourceLocation*>(location.get()))
                {
                    if (key == "response")
                    {
                        parser.require_identifier_annotation({ "http" });
                        res_location->response = parser.parse_token_as_number<int>(parser.value().token);
                    }
                    else if (key == "content")
                    {
                        res_location->content = parser.parse_value<TextDocument>();
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
                        parser.require_no_annotation();
                        redir_location->redirect_url = parser.parse_token_as_string(parser.value().token);
                    }
                    else if (key == "response")
                    {
                        parser.require_identifier_annotation({ "http" });
                        redir_location->response = parser.parse_token_as_number<int>(parser.value().token);
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
                        parser.require_no_annotation();
                        static_location->root_directory = parser.parse_token_as_string(parser.value().token);
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
                throw jxc::parse_error(jxc::format("Unexpected key {} in {} location", jxc::detail::debug_string_repr(key), loc_anno), parser.value());
            }
        }

        return location;
    }
};


template<>
struct jxc::Converter<LogTarget>
{
    using value_type = LogTarget;
    static const jxc::TokenList& get_annotation()
    {
        static const jxc::TokenList anno = jxc::TokenList::from_identifier("LogTarget");
        return anno;
    }

    static LogTarget parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        LogTarget result;

        TokenView anno = parser.get_value_annotation(generic_anno);
        if (!anno || !anno.equals_annotation_string_lexed("logger"))
        {
            throw jxc::parse_error("Annotation 'logger' required", parser.value());
        }

        parser.require(jxc::ElementType::BeginObject);

        while (parser.next())
        {
            if (parser.value().type == jxc::ElementType::EndObject)
            {
                break;
            }

            // get the object key and advance to the value
            parser.require(jxc::ElementType::ObjectKey);
            const std::string key = parser.parse_token_as_object_key<std::string>(parser.value().token);
            parser.require_next();

            if (key == "dest")
            {
                TokenView dest_anno = parser.value().annotation;
                const std::string dest_anno_str = std::string(parser.require_identifier_annotation(dest_anno, { "file", "stream" }, true));

                const jxc::ElementType dest_element_type = parser.value().type;
                switch (dest_element_type)
                {
                case jxc::ElementType::BeginExpression:
                {
                    if (dest_anno_str != "stream")
                    {
                        throw jxc::parse_error("`stream` annotation requires expression type", parser.value());
                    }

                    parser.require_next();
                    parser.require(jxc::ElementType::ExpressionToken);

                    const std::string dest_value = parser.parse_token_as_string(parser.value().token);
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
                        throw jxc::parse_error(jxc::format("Expected identifier `stdout` or `stderr`, got {}", jxc::detail::debug_string_repr(dest_value)), parser.value());
                    }

                    parser.require_next();
                    parser.require(jxc::ElementType::EndExpression);
                    break;
                }

                case jxc::ElementType::String:
                    if (dest_anno_str != "file")
                    {
                        throw jxc::parse_error("`file` annotation requires string type", parser.value());
                    }

                    result.type = LogDestType::LogFile;
                    result.log_file_path = parser.parse_token_as_string(parser.value().token);
                    break;

                default:
                    throw jxc::parse_error(jxc::format("Expected expression or string, got {}", jxc::element_type_to_string(dest_element_type)), parser.value());
                }
            }
            else if (key == "format")
            {
                result.format_func = parser.parse_value<LogFormatFunc>();
            }
            else
            {
                throw jxc::parse_error(jxc::format("Invalid log target key {}", jxc::detail::debug_string_repr(key)), parser.value());
            }
        }

        return result;
    }
};


template<>
struct jxc::Converter<LogFormatFunc>
{
    using value_type = LogFormatFunc;

    static value_type parse(jxc::conv::Parser& parser, TokenView generic_anno)
    {
        if (parser.value().type != jxc::ElementType::BeginExpression)
        {
            throw jxc::parse_error(jxc::format("Expected log format to be an expression, got {}", jxc::element_type_to_string(parser.value().type)), parser.value());
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

        ErrorInfo err;

        while (parser.next())
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
                    if (!jxc::util::parse_string_token<std::string>(tok, header_name, err))
                    {
                        throw jxc::parse_error("Invalid string token", err);
                    }
                    break;
                default:
                    throw jxc::parse_error(jxc::format("Expected token following @ to be an identifier or string, got {}", jxc::token_type_to_string(tok.type)), tok);
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
                            throw jxc::parse_error(jxc::format("Invalid log format variable name {}", jxc::detail::debug_string_repr(var_name)), tok);
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
                    if (!jxc::util::parse_string_token<std::string>(tok, str_value, err))
                    {
                        throw jxc::parse_error("Invalid string token", err);
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
};


// End Converter implementations


struct WebServerConfig
{
    std::string listen_host;
    uint16_t listen_port = 8080;
    uint32_t worker_threads = 8;
    TimeDelta read_timeout;
    TimeDelta write_timeout;
    TimeDelta idle_interval;
    DataSizeBytes payload_max_length = make_data_size_from_suffix(8, "mb").value();
    LogTarget access_log;
    LogTarget error_log;
    TextDocument default_not_found_page;
    TextDocument default_server_error_page;
    std::vector<std::shared_ptr<BaseLocation>> locations;
    MimeTypeMap mime_types;

    // call after parsing to fix up the data before using
    void prepare()
    {
        auto fixup_document = [&](TextDocument& doc)
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
        ss << "\tpayload_max_length: " << payload_max_length << " bytes\n";

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


JXC_DEFINE_STRUCT_CONVERTER(
    WebServerConfig,
    jxc::def_struct<WebServerConfig>("WebServerConfig")
        .def_field("listen_host", &WebServerConfig::listen_host)
        .def_field("listen_port", &WebServerConfig::listen_port, jxc::FieldFlags::Optional)
        .def_field("worker_threads", &WebServerConfig::worker_threads, jxc::FieldFlags::Optional)
        .def_field("read_timeout", &WebServerConfig::read_timeout)
        .def_field("write_timeout", &WebServerConfig::write_timeout)
        .def_field("idle_interval", &WebServerConfig::idle_interval)
        .def_field("payload_max_length", &WebServerConfig::payload_max_length, jxc::FieldFlags::Optional,
            [](jxc::conv::Parser& parser, const std::string& field_key, WebServerConfig& out_value)
            {
                parser.require(jxc::ElementType::Number);
                std::string suffix;
                const uint64_t value = parser.parse_token_as_number<uint64_t>(parser.value().token, { "b", "kb", "mb", "gb", "tb" }, suffix);
                out_value.payload_max_length = make_data_size_from_suffix(value, suffix).value();
            })
        .def_field("access_log", &WebServerConfig::access_log, jxc::FieldFlags::Optional)
        .def_field("error_log", &WebServerConfig::error_log, jxc::FieldFlags::Optional)
        .def_field("default_not_found_page", &WebServerConfig::default_not_found_page, jxc::FieldFlags::Optional)
        .def_field("default_server_error_page", &WebServerConfig::default_server_error_page, jxc::FieldFlags::Optional)
        .def_field("mime_types", &WebServerConfig::mime_types)
    
        // "Property" indicates that this isn't an actual field on the struct.
        // We want to allow multiple "location" keys, and each time we encounter one, add it to the locations array.
        .def_property("location", jxc::FieldFlags::Optional | jxc::FieldFlags::AllowMultiple,
            [](jxc::conv::Parser& parser, const std::string& field_key, WebServerConfig& out_value)
            {
                out_value.locations.push_back(jxc::Converter<std::shared_ptr<BaseLocation>>::parse(parser, TokenView()));
            }
        )
);


struct ParsedArgs
{
    fs::path config_path = "./web_server_converter_config.jxc";
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

    // parse the config file
    WebServerConfig cfg;
    try
    {
        cfg = jxc::conv::parse<WebServerConfig>(config_file_data);
    }
    catch (const jxc::parse_error& err)
    {
        std::cerr << "Failed parsing config file: " << err.error_info_to_string(config_file_data) << std::endl;
        return 1;
    }

    // clean up config inputs
    cfg.prepare();

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
            const TextDocument& doc = (res.status == 404) ? cfg.default_not_found_page : cfg.default_server_error_page;
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

