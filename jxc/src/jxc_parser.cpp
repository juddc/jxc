#include "jxc/jxc_parser.h"
#include "fastfloat.h"


namespace jxc
{

const char* element_type_to_string(ElementType type)
{
    switch (type)
    {
    case JXC_ENUMSTR(ElementType, Invalid);
    case JXC_ENUMSTR(ElementType, Number);
    case JXC_ENUMSTR(ElementType, Bool);
    case JXC_ENUMSTR(ElementType, Null);
    case JXC_ENUMSTR(ElementType, Bytes);
    case JXC_ENUMSTR(ElementType, String);
    case JXC_ENUMSTR(ElementType, DateTime);
    case JXC_ENUMSTR(ElementType, ExpressionToken);
    case JXC_ENUMSTR(ElementType, Comment);
    case JXC_ENUMSTR(ElementType, BeginArray);
    case JXC_ENUMSTR(ElementType, EndArray);
    case JXC_ENUMSTR(ElementType, BeginExpression);
    case JXC_ENUMSTR(ElementType, EndExpression);
    case JXC_ENUMSTR(ElementType, BeginObject);
    case JXC_ENUMSTR(ElementType, ObjectKey);
    case JXC_ENUMSTR(ElementType, EndObject);
    }
    return "UnknownElementType";
}


#if JXC_ENABLE_JUMP_BLOCK_PROFILER
struct BlockProfilerInfo
{
    int32_t num_runs = 0;
    int64_t total_duration_ns = 0;
    size_t max_stack_depth = 0;
};

static std::unordered_map<std::string, BlockProfilerInfo> s_block_profiler_data;

struct JumpParserProfiler
{
    detail::Timer timer;
    std::string block_name;
    size_t stack_depth = 0;

    explicit JumpParserProfiler(const char* in_block_name, const char* in_context = nullptr, size_t stack_depth = 0)
        : block_name((in_context != nullptr) ? jxc::format("{}.{}", in_block_name, in_context) : std::string(in_block_name))
        , stack_depth(stack_depth)
    {
        timer.reset();
    }

    ~JumpParserProfiler()
    {
        const int64_t runtime_ns = timer.elapsed().count();
        auto iter = s_block_profiler_data.find(block_name);
        BlockProfilerInfo* info = nullptr;
        if (iter != s_block_profiler_data.end())
        {
            info = &iter->second;
        }
        else
        {
            s_block_profiler_data.insert({ block_name, BlockProfilerInfo{} });
            info = &s_block_profiler_data[block_name];
        }

        info->num_runs++;
        info->total_duration_ns += runtime_ns;
        if (stack_depth > info->max_stack_depth)
        {
            info->max_stack_depth = stack_depth;
        }
    }

    JumpParserProfiler(const JumpParserProfiler&) = delete;
    JumpParserProfiler& operator=(const JumpParserProfiler&) = delete;
    JumpParserProfiler(JumpParserProfiler&&) = delete;
    JumpParserProfiler& operator=(JumpParserProfiler&&) = delete;
};
#endif


void JumpParser::reset_profiler()
{
#if JXC_ENABLE_JUMP_BLOCK_PROFILER
    s_block_profiler_data.clear();
#endif
}


#if JXC_ENABLE_JUMP_BLOCK_PROFILER
std::string JumpParser::get_profiler_results(bool sort_by_runtime)
{
    std::vector<std::string> sorted_keys;
    for (const auto& pair: s_block_profiler_data)
    {
        sorted_keys.push_back(pair.first);
    }

    if (sort_by_runtime)
    {
        std::sort(sorted_keys.begin(), sorted_keys.end(),
            [](const std::string& a, const std::string& b)
            {
                return s_block_profiler_data[a].total_duration_ns > s_block_profiler_data[b].total_duration_ns;
            });
    }
    else
    {
        std::sort(sorted_keys.begin(), sorted_keys.end());
    }

    std::ostringstream ss;

    for (const std::string& key : sorted_keys)
    {
        const BlockProfilerInfo& info = s_block_profiler_data[key];
        ss << jxc::format("{}: calls: {}, total_runtime: {}ns (avg runtime per call: {}ns), max_stack_depth: {}\n",
            key,
            info.num_runs,
            info.total_duration_ns, //detail::Timer::ns_to_ms(info.total_duration_ns),
            (info.num_runs > 0) ? (info.total_duration_ns / info.num_runs) : 0,
            info.max_stack_depth);
    }

    return ss.str();
}
#else
std::string JumpParser::get_profiler_results(bool) { return std::string{}; }
#endif

#define JP_PASTE(A, B) A ## B
#define JP_CONCAT(A, B) JP_PASTE(A, B)
#define JP_MAKE_GOTO(NAME) JP_CONCAT(NAME ## _, __LINE__)

#define JP_MAKE_ERROR(ERR_MSG) \
        ErrorInfo{ jxc::format("[{}:{} {}():{}] {}", jxc::detail::get_base_filename(__FILE__), __LINE__, __FUNCTION__, cur_jump_block_name, (ERR_MSG)), \
            tok.start_idx, tok.end_idx }

#define JP_MAKE_ERRORF(FMT_STRING, ...) \
        ErrorInfo{ jxc::format("[{}:{} {}():{}] " FMT_STRING, jxc::detail::get_base_filename(__FILE__), __LINE__, __FUNCTION__, cur_jump_block_name, __VA_ARGS__), \
            tok.start_idx, tok.end_idx }

#define JP_ERROR(ERR_MSG) do { error = JP_MAKE_ERROR(ERR_MSG); goto jp_end; } while(0)
#define JP_ERRORF(FMT_STRING, ...) do { error = JP_MAKE_ERRORF(FMT_STRING, __VA_ARGS__); goto jp_end; } while(0)

#if JXC_ENABLE_JUMP_BLOCK_PROFILER
#define JP_BLOCK_TIMER() JumpParserProfiler JP_CONCAT(_block_profiler_, __LINE__) { cur_jump_block_name, nullptr, jump_stack.size() }
#define JP_BLOCK_TIMER_CTX(CTX_NAME) JumpParserProfiler JP_CONCAT(_block_profiler_, __LINE__) { cur_jump_block_name, #CTX_NAME, jump_stack.size() }
#else
#define JP_BLOCK_TIMER()
#define JP_BLOCK_TIMER_CTX(CTX_NAME)
#endif

bool JumpParser::advance_separator(TokenType container_close_type, const char* cur_jump_block_name)
{
    JP_BLOCK_TIMER_CTX(advance_separator);
    bool found_comma = false;
    int found_linebreaks = 0;
    while (true)
    {
        if (tok.type == container_close_type)
        {
            return true;
        }

        switch (tok.type)
        {
        case TokenType::Comma:
            if (found_comma)
            {
                error = JP_MAKE_ERROR("Found multiple commas while scanning for separator");
                return false;
            }
            found_comma = true;
            if (!advance_skip_comments()) { return false; }
            break;

        case TokenType::LineBreak:
            ++found_linebreaks;
            if (!advance_skip_comments()) { return false; }
            break;

        case TokenType::Comment:
            if (!advance_skip_comments()) { return false; }
            break;

        default:
            if (found_comma || found_linebreaks > 0)
            {
                return true;
            }
            else
            {
                error = JP_MAKE_ERROR("Missing separator");
                return false;
            }
        }
    }
}


bool JumpParser::next()
{
#if JXC_ENABLE_JUMP_BLOCK_PROFILER
    JumpParserProfiler _profiler_root_("next()", nullptr, jump_stack.size());
#endif

    const char* cur_jump_block_name = "INVALID";
    tok.reset();
    annotation_buffer.clear();

#define JP_ADVANCE() do { if (!advance()) { goto jp_end; } } while(0)

#define JP_ADVANCE_SKIP_COMMENTS() do { if (!advance_skip_comments()) { goto jp_end; } } while(0)

#define JP_SKIP_OVER_LINE_BREAKS() do { if (!skip_over_line_breaks()) { goto jp_end; } } while(0)

#define JP_ADVANCE_SEPARATOR(CONTAINER_CLOSE_TOK_TYPE) do { if (!advance_separator(CONTAINER_CLOSE_TOK_TYPE, cur_jump_block_name)) { goto jp_end; } } while(0)

#define JP_YIELD(ELEMENT_TYPE) do { \
        current_value.type = (ELEMENT_TYPE); \
        current_value.token = tok; \
        current_value.annotation.num_tokens = annotation_buffer.size(); \
        current_value.annotation.start = (current_value.annotation.num_tokens > 0) ? &annotation_buffer.front() : nullptr; \
        return (ELEMENT_TYPE) != ElementType::Invalid; \
    } while(0)

#define JP_JUMP_BLOCK_BEGIN(NAME) NAME: cur_jump_block_name = #NAME
#define JP_JUMP_BLOCK_END(NAME) do { JXC_DEBUG_ASSERT(std::string_view(cur_jump_block_name) == std::string_view(#NAME)); goto jp_jump_block_reached_end_without_yield; } while(0)

#define JP_GOTO_PARSE_VALUE(TOK_TYPE, PRE_VALUE_EXPR) do { \
    switch (TOK_TYPE) { \
    case TokenType::ExclamationPoint: \
    case TokenType::Identifier: \
        (PRE_VALUE_EXPR); \
        goto jp_value_with_annotation; \
    case TokenType::True: \
    case TokenType::False: \
    case TokenType::Null: \
    case TokenType::Number: \
    case TokenType::String: \
    case TokenType::ByteString: \
    case TokenType::DateTime: \
    case TokenType::SquareBracketOpen: \
    case TokenType::BraceOpen: \
    case TokenType::ParenOpen: \
        (PRE_VALUE_EXPR); \
        goto jp_value_without_annotation; \
    default: \
        JP_ERRORF("Expected annotation or value, got {} token", token_type_to_string(TOK_TYPE)); \
    } \
} while(0)

#define JP_PARSE_DOTTED_IDENT() do { \
        while (tok.type == TokenType::Identifier) { \
            annotation_buffer.push(tok); \
            JP_ADVANCE_SKIP_COMMENTS(); \
            if (tok.type != TokenType::Period) { goto JP_MAKE_GOTO(jp_post_parse_dotted_indent); } \
            annotation_buffer.push(tok); \
            JP_ADVANCE_SKIP_COMMENTS(); \
        } \
        JP_ERROR("Annotations may not end with a period"); \
    } while(0); \
JP_MAKE_GOTO(jp_post_parse_dotted_indent):


    // Ideally this should be `JP_JUMP_BLOCK_BEGIN(jp_top);`, but since we don't call `goto jp_top` anywhere,
    // it causes the compiler to complain.
    cur_jump_block_name = "jp_top";
    {
        JP_BLOCK_TIMER();
        if (jump_stack.size() > 0)
        {
            switch (jump_vars->state)
            {
            case JS_Value:
                JP_ADVANCE_SKIP_COMMENTS();
                goto jp_value_with_annotation;

            case JS_Array:
                JP_ADVANCE_SKIP_COMMENTS();
                goto jp_arrayvalue;

            case JS_Expr:
                JP_ADVANCE();
                goto jp_expritem;

            case JS_Object:
                JP_ADVANCE_SKIP_COMMENTS();
                switch (jump_vars->container_state)
                {
                case OBJ_Begin: goto jp_objectbegin;
                case OBJ_Key: goto jp_objectkey;
                case OBJ_Value: goto jp_objectvalue;
                default: JP_ERROR("Invalid parser object state");
                }

            default:
                JP_ERROR("Invalid parser state");
            }
        }
        else
        {
            JP_ADVANCE_SKIP_COMMENTS();
            JP_SKIP_OVER_LINE_BREAKS();
            JP_GOTO_PARSE_VALUE(tok.type, (void)0);
        }
    }
    JP_JUMP_BLOCK_END(jp_top);

    JP_JUMP_BLOCK_BEGIN(jp_value_with_annotation);
    {
        JP_BLOCK_TIMER();
        // parse annotation first?
        if (tok.type == TokenType::ExclamationPoint || tok.type == TokenType::Identifier)
        {
            JXC_DEBUG_ASSERT(annotation_buffer.size() == 0);

            if (tok.type == TokenType::ExclamationPoint)
            {
                annotation_buffer.push(tok);
                JP_ADVANCE_SKIP_COMMENTS();

                // if the token after the `!` is not an identifier, then it's not part of the annotation, so we must be done.
                if (tok.type != TokenType::Identifier)
                {
                    goto jp_value_without_annotation;
                }
            }

            JP_PARSE_DOTTED_IDENT()

            // if we don't have an angle bracket next, then we're done with the annotation
            if (tok.type != TokenType::AngleBracketOpen)
            {
                // no need to advance here - parse_dotted_identifier already did that
                goto jp_value_without_annotation;
            }

            int32_t angle_bracket_depth = 1;
            int32_t paren_depth = 0;
            annotation_buffer.push(tok);

            while (angle_bracket_depth > 0)
            {
                JP_ADVANCE_SKIP_COMMENTS();

jp_handle_annotation_type:
                switch (tok.type)
                {
                // identifiers and certain symbol types can always be used in annotations:
                case TokenType::Identifier:
                    JP_PARSE_DOTTED_IDENT()
                    // parse_dotted_identifier needs to set tok to the token _after_ the identifier, because it needs to look at to determine
                    // if it's done parsing the identifier or not. if we used `continue` here instead of `goto`, it would advance a second
                    // time and skip a token.
                    goto jp_handle_annotation_type;

                case TokenType::ExclamationPoint:
                case TokenType::Asterisk:
                case TokenType::QuestionMark:
                case TokenType::Pipe:
                case TokenType::Ampersand:
                    annotation_buffer.push(tok);
                    break;

                    // allow more angle brackets, but track depth
                case TokenType::AngleBracketOpen:
                    angle_bracket_depth += 1;
                    if (angle_bracket_depth > JumpStackVars::max_bracket_depth)
                    {
                        JP_ERROR("Reached max limit for angle bracket depth while parsing annotation");
                    }
                    annotation_buffer.push(tok);
                    break;

                case TokenType::AngleBracketClose:
                    angle_bracket_depth -= 1;
                    if (angle_bracket_depth < 0)
                    {
                        JP_ERROR("Unmatched angle brackets while parsing annotation");
                    }
                    annotation_buffer.push(tok);
                    break;

                    // allow parens, but track depth
                case TokenType::ParenOpen:
                    paren_depth += 1;
                    if (paren_depth > JumpStackVars::max_bracket_depth)
                    {
                        JP_ERROR("Reached max limit for parentheses depth while parsing annotation");
                    }
                    annotation_buffer.push(tok);
                    break;

                case TokenType::ParenClose:
                    paren_depth -= 1;
                    if (paren_depth < 0)
                    {
                        JP_ERROR("Unmatched parentheses while parsing annotation");
                    }
                    annotation_buffer.push(tok);
                    break;

                case TokenType::Equals:
                case TokenType::Comma:
                    annotation_buffer.push(tok);
                    break;

                    // value types
                case TokenType::True:
                case TokenType::False:
                case TokenType::Null:
                case TokenType::Number:
                case TokenType::String:
                case TokenType::ByteString:
                case TokenType::DateTime:
                    annotation_buffer.push(tok);
                    break;

                default:
                    JP_ERRORF("Unexpected token {} while parsing annotation", tok.to_string());
                }
            }

            JXC_DEBUG_ASSERT(angle_bracket_depth == 0);

            if (paren_depth != 0)
            {
                JP_ERROR("Unmatched parentheses while parsing annotation");
            }

            // advance to the token _after_ the annotation
            JP_ADVANCE_SKIP_COMMENTS();

            JXC_DEBUG_ASSERT(annotation_buffer.size() > 0);
            JXC_DEBUG_ASSERT(annotation_buffer.back() != tok);
        }

        // annotation complete or skipped. either way, parse the value next.
        goto jp_value_without_annotation;
    }
    JP_JUMP_BLOCK_END(jp_value_with_annotation);

    JP_JUMP_BLOCK_BEGIN(jp_value_without_annotation);
    {
        JP_BLOCK_TIMER();
        // skip over any line breaks before the value
        JP_SKIP_OVER_LINE_BREAKS();

        // parse the value
        switch (tok.type)
        {
        case TokenType::True:
        case TokenType::False:
            JP_YIELD(ElementType::Bool);
        case TokenType::Null:
            JP_YIELD(ElementType::Null);
        case TokenType::Number:
            JP_YIELD(ElementType::Number);
        case TokenType::ByteString:
            JP_YIELD(ElementType::Bytes);
        case TokenType::String:
            JP_YIELD(ElementType::String);
        case TokenType::DateTime:
            JP_YIELD(ElementType::DateTime);
        case TokenType::SquareBracketOpen:
            goto jp_arraybegin;
        case TokenType::ParenOpen:
            goto jp_exprbegin;
        case TokenType::BraceOpen:
            goto jp_objectbegin;
        case TokenType::EndOfStream:
            goto jp_end;
        default:
            JP_ERRORF("Unexpected token {} {} while parsing value", token_type_to_string(tok.type), detail::debug_string_repr(tok.to_string()));
        }
    }
    JP_JUMP_BLOCK_END(jp_value_without_annotation);

    JP_JUMP_BLOCK_BEGIN(jp_arraybegin);
    {
        JP_BLOCK_TIMER();
        JXC_DEBUG_ASSERT(tok.type == TokenType::SquareBracketOpen);
        jump_stack_push(JS_Array);
        JP_YIELD(ElementType::BeginArray);
    }
    JP_JUMP_BLOCK_END(jp_arraybegin);

    JP_JUMP_BLOCK_BEGIN(jp_arrayvalue);
    {
        JP_BLOCK_TIMER();
        if (jump_vars->container_size <= 0)
        {
            // empty array

            // optional linebreak before value or array close
            JP_SKIP_OVER_LINE_BREAKS();

            if (tok.type == TokenType::SquareBracketClose)
            {
                goto jp_arrayend;
            }
            else
            {
                JP_GOTO_PARSE_VALUE(tok.type, ++(jump_vars->container_size));
            }
        }
        else
        {
            // at least one value in the array
            JP_ADVANCE_SEPARATOR(TokenType::SquareBracketClose);

            if (tok.type == TokenType::SquareBracketClose)
            {
                goto jp_arrayend;
            }
            else
            {
                JP_GOTO_PARSE_VALUE(tok.type, ++(jump_vars->container_size));
            }
        }
    }
    JP_JUMP_BLOCK_END(jp_arrayvalue);

    JP_JUMP_BLOCK_BEGIN(jp_arrayend);
    {
        JP_BLOCK_TIMER();
        JXC_DEBUG_ASSERT(tok.type == TokenType::SquareBracketClose);
        JXC_DEBUG_ASSERT(jump_vars->state == JS_Array);
        jump_stack_pop();
        JP_YIELD(ElementType::EndArray);
    }
    JP_JUMP_BLOCK_END(jp_arrayend);

    JP_JUMP_BLOCK_BEGIN(jp_exprbegin);
    {
        JP_BLOCK_TIMER();
        JXC_DEBUG_ASSERT(tok.type == TokenType::ParenOpen);
        jump_stack_push(JS_Expr);
        jump_vars->paren_depth = 1;
        JP_YIELD(ElementType::BeginExpression);
    }
    JP_JUMP_BLOCK_END(jp_exprbegin);

    JP_JUMP_BLOCK_BEGIN(jp_expritem);
    {
        JP_BLOCK_TIMER();
        switch (tok.type)
        {
        // values
        case TokenType::True:
        case TokenType::False:
            JP_YIELD(ElementType::Bool);
        case TokenType::Null:
            JP_YIELD(ElementType::Null);
        case TokenType::Number:
            JP_YIELD(ElementType::Number);
        case TokenType::String:
            JP_YIELD(ElementType::String);
        case TokenType::DateTime:
            JP_YIELD(ElementType::DateTime);
        case TokenType::ByteString:
            JP_YIELD(ElementType::Bytes);

        // generic "expression tokens"
        case TokenType::Identifier:
        case TokenType::Comma:
        case TokenType::Colon:
        case TokenType::AtSymbol:
        case TokenType::LineBreak:
        case TokenType::Pipe:
        case TokenType::Ampersand:
        case TokenType::ExclamationPoint:
        case TokenType::Equals:
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Asterisk:
        case TokenType::Slash:
        case TokenType::Backslash:
        case TokenType::Percent:
        case TokenType::Caret:
        case TokenType::Period:
        case TokenType::QuestionMark:
        case TokenType::Tilde:
        case TokenType::Backtick:
        case TokenType::Semicolon:
            JP_YIELD(ElementType::ExpressionToken);

        case TokenType::Comment:
            JP_YIELD(ElementType::Comment);

        // angle brackets do NOT have matching pairs enforced so they can be used
        // for boolean expressions if needed
        case TokenType::AngleBracketOpen:
        case TokenType::AngleBracketClose:
            JP_YIELD(ElementType::ExpressionToken);

        // square brackets, curly braces, and parens are just expression tokens,
        // but we always require matching pairs
        case TokenType::SquareBracketOpen:
            ++jump_vars->square_bracket_depth;
            if (jump_vars->square_bracket_depth > JumpStackVars::max_bracket_depth)
            {
                JP_ERROR("Reached max limit for square bracket depth");
            }
            JP_YIELD(ElementType::ExpressionToken);
        case TokenType::SquareBracketClose:
            --jump_vars->square_bracket_depth;
            if (jump_vars->square_bracket_depth < 0)
            {
                JP_ERROR("Unmatched square bracket while parsing expression");
            }
            JP_YIELD(ElementType::ExpressionToken);
        case TokenType::BraceOpen:
            ++jump_vars->brace_depth;
            if (jump_vars->brace_depth > JumpStackVars::max_bracket_depth)
            {
                JP_ERROR("Reached max limit for curly brace depth");
            }
            JP_YIELD(ElementType::ExpressionToken);
        case TokenType::BraceClose:
            --jump_vars->brace_depth;
            if (jump_vars->brace_depth < 0)
            {
                JP_ERROR("Unmatched curly brace while parsing expression");
            }
            JP_YIELD(ElementType::ExpressionToken);
        case TokenType::ParenOpen:
            ++jump_vars->paren_depth;
            if (jump_vars->paren_depth > JumpStackVars::max_bracket_depth)
            {
                JP_ERROR("Reached max limit for parentheses depth");
            }
            JP_YIELD(ElementType::ExpressionToken);
        case TokenType::ParenClose:
            --jump_vars->paren_depth;
            if (jump_vars->paren_depth < 0)
            {
                JP_ERROR("Unmatched parentheses while parsing expression");
            }
            else if (jump_vars->paren_depth == 0)
            {
                goto jp_exprend;
            }
            JP_YIELD(ElementType::ExpressionToken);
        default:
            JP_ERROR("Unexpected token while parsing expression");
        }
    }
    JP_JUMP_BLOCK_END(jp_expritem);

    JP_JUMP_BLOCK_BEGIN(jp_exprend);
    {
        JP_BLOCK_TIMER();
        JXC_DEBUG_ASSERT(tok.type == TokenType::ParenClose);
        JXC_DEBUG_ASSERT(jump_vars->state == JS_Expr);
        jump_stack_pop();
        JP_YIELD(ElementType::EndExpression);
    }
    JP_JUMP_BLOCK_END(jp_exprend);

    JP_JUMP_BLOCK_BEGIN(jp_objectbegin);
    {
        JP_BLOCK_TIMER();
        JXC_DEBUG_ASSERT(tok.type == TokenType::BraceOpen);
        jump_stack_push(JS_Object, OBJ_Key);
        JP_YIELD(ElementType::BeginObject);
    }
    JP_JUMP_BLOCK_END(jp_objectbegin);

    JP_JUMP_BLOCK_BEGIN(jp_objectkey);
    {
        JP_BLOCK_TIMER();
        if (jump_vars->container_size > 0)
        {
            JP_ADVANCE_SEPARATOR(TokenType::BraceClose);
            if (tok.type == TokenType::BraceClose)
            {
                goto jp_objectend;
            }
        }

        // allowed extra line breaks before the key starts
        JP_SKIP_OVER_LINE_BREAKS();

        switch (tok.type)
        {
        case TokenType::Comment:
            JP_YIELD(ElementType::Comment);
        case TokenType::BraceClose:
            goto jp_objectend;
        case TokenType::String:
        case TokenType::Null:
        case TokenType::Number:
        case TokenType::True:
        case TokenType::False:
        case TokenType::Identifier:
            jump_vars->container_state = OBJ_Value;
            JP_YIELD(ElementType::ObjectKey);
        default:
            JP_ERROR("Expected object key");
        }
    }
    JP_JUMP_BLOCK_END(jp_objectkey);

    JP_JUMP_BLOCK_BEGIN(jp_objectvalue);
    {
        JP_BLOCK_TIMER();
        JP_SKIP_OVER_LINE_BREAKS();

        if (tok.type == TokenType::Colon)
        {
            JP_ADVANCE_SKIP_COMMENTS();
        }
        else
        {
            JP_ERROR("Expected colon after object key");
        }

        // line breaks after the colon are allowed
        JP_SKIP_OVER_LINE_BREAKS();

        // parse the value that goes with this key
        jump_vars->container_state = OBJ_Key;
        JP_GOTO_PARSE_VALUE(tok.type, ++(jump_vars->container_size));
    }
    JP_JUMP_BLOCK_END(jp_objectvalue);

    JP_JUMP_BLOCK_BEGIN(jp_objectend);
    {
        JP_BLOCK_TIMER();
        JXC_DEBUG_ASSERT(tok.type == TokenType::BraceClose);
        JXC_DEBUG_ASSERT(jump_vars->state == JS_Object);
        jump_stack_pop();
        JP_YIELD(ElementType::EndObject);
    }
    JP_JUMP_BLOCK_END(jp_objectend);

jp_end:
    current_value.reset();
    return false;

jp_jump_block_reached_end_without_yield:
    // should never get here - it's just to check against control flow bugs
    JP_ERROR("Reached end of jump block unexpectedly");

#undef JP_ADVANCE
#undef JP_ERROR
#undef JP_ERRORF
}


namespace util
{


bool string_is_number_base_10(std::string_view value)
{
    if (value.size() == 0 || (value.size() == 1 && !is_decimal_digit(value[0])))
    {
        return false;
    }

    if (value[0] == '-' || value[0] == '+')
    {
        value = value.substr(1);
    }

    bool found_decimal_pt = false;
    for (size_t i = 0; i < value.size(); i++)
    {
        if (!found_decimal_pt && value[i] == '.')
        {
            if (i == 0 || i == value.size() - 1)
            {
                // decimal point as the first or last char - invalid
                return false;
            }
            found_decimal_pt = true;
        }
        else if (!is_decimal_digit(value[i]))
        {
            return false;
        }
    }

    return true;
}


bool string_to_double(std::string_view value, double& out_value)
{
    // fast_float does not allow a leading '+' char
    if (detail::string_view_starts_with(value, '+'))
    {
        value = value.substr(1);
    }

    switch (value.size())
    {
    case 0:
        out_value = 0.0;
        return true;
    case 1:
        if (is_decimal_digit(value[0]))
        {
            out_value = static_cast<double>(char_to_int(value[0]));
            return true;
        }
        else
        {
            // no other way to interpret a single-character number
            return false;
        }
    case 2:
        if (value[0] == '-' && is_decimal_digit(value[1]))
        {
            out_value = static_cast<double>(-char_to_int(value[1]));
            return true;
        }
        break;
    case 3:
        if (is_decimal_digit(value[0]) && value[1] == '.' && value[2] == '0')
        {
            out_value = static_cast<double>(char_to_int(value[0]));
            return true;
        }
        break;
    case 4:
        if (value[0] == '-' && is_decimal_digit(value[1]) && value[2] == '.' && value[3] == '0')
        {
            out_value = static_cast<double>(-char_to_int(value[1]));
            return true;
        }
        break;
    default:
        break;
    }

    const fast_float::from_chars_result result = fast_float::from_chars<double>(value.data(), value.data() + value.size(), out_value);
    if (result.ec == std::errc(0))
    {
        return true;
    }
    return false;
}


bool split_number_token_value(const Token& number_token, NumberTokenSplitResult& out_result, ErrorInfo& out_error)
{
    out_result.float_type = FloatLiteralType::Finite;
    out_result.sign = '+';
    out_result.exponent = 0;

    if (number_token.type != TokenType::Number)
    {
        out_error = ErrorInfo{
            jxc::format("Failed to parse number - expected token of type Number, got {}", token_type_to_string(number_token.type)),
            number_token.start_idx, number_token.end_idx };
        return false;
    }

    std::string_view value = number_token.value.as_view();

    if (value.size() == 0)
    {
        out_error = ErrorInfo{ "Number token value is empty", number_token.start_idx, number_token.end_idx };
        return false;
    }
    else if (value.size() == 1)
    {
        out_result.prefix = {};
        out_result.value = value;
        out_result.suffix = {};
        if (!is_decimal_digit(value[0]))
        {
            out_error = ErrorInfo{
                jxc::format("Failed to parse number: `{}` is not a digit", value[0]),
                number_token.start_idx, number_token.end_idx };
            return false;
        }
        return true;
    }

    // read sign prefix
    if (value[0] == '-' || value[0] == '+')
    {
        out_result.sign = value[0];
        value = value.substr(1);
    }

    // check for float literal types
    if (value.size() == 3)
    {
        if (value == "inf")
        {
            out_result.float_type = (out_result.sign == '-') ? FloatLiteralType::NegInfinity : FloatLiteralType::PosInfinity;
            return true;
        }
        else if (value == "nan")
        {
            out_result.float_type = FloatLiteralType::NotANumber;
            return true;
        }
    }

    // NB. (`d` isn't an actual supported prefix, it's just used internally in this function)
    char number_type = 'd'; // d=decimal, x=hex, b=binary, o=octal

    // get the '0x' type prefix if there is one
    if (value.size() > 2 && value[0] == '0' && value[1] != '.' && !is_decimal_digit(value[1]))
    {
        const char ch = value[1];
        switch (ch)
        {
        case 'x':
        case 'X':
        case 'b':
        case 'B':
        case 'o':
        case 'O':
            number_type = ch;
            out_result.prefix = value.substr(0, 2);
            value = value.substr(2);
            break;
        default:
            out_error = ErrorInfo{ jxc::format("Invalid syntax for number literal. Expected prefix like '0x', got '0{}'", ch),
                number_token.start_idx, number_token.end_idx };
            return false;
        }
    }
    else
    {
        out_result.prefix = std::string_view{};
    }

    size_t idx = 0;
    size_t value_len = 0;

    switch (number_type)
    {
    case 'd':
    {
        // integer part
        while (idx < value.size() && is_decimal_digit(value[idx]))
        {
            ++idx;
            ++value_len;
        }

        // fraction part
        if (idx < value.size() && value[idx] == '.')
        {
            ++idx;
            ++value_len;

            while (idx < value.size() && is_decimal_digit(value[idx]))
            {
                ++idx;
                ++value_len;
            }
        }

        break;
    }
    case 'x':
    case 'X':
    {
        while (idx < value.size() && is_hex_digit(value[idx]))
        {
            ++idx;
            ++value_len;
        }
        break;
    }
    case 'b':
    case 'B':
    {
        while (idx < value.size() && is_binary_digit(value[idx]))
        {
            ++idx;
            ++value_len;
        }
        break;
    }
    case 'o':
    case 'O':
    {
        while (idx < value.size() && is_octal_digit(value[idx]))
        {
            ++idx;
            ++value_len;
        }
        break;
    }
    default:
        JXC_DEBUG_ASSERTF(false, "Invalid number type {}", number_type);
        break;
    }

    // exponent
    if (number_type == 'd'
        && idx < value.size() - 1  // minimum 2 chars
        && (value[idx] == 'e' || value[idx] == 'E')
        && (value[idx + 1] == '-' || value[idx + 1] == '+' || is_decimal_digit(value[idx + 1])))
    {
        ++idx;

        bool exponent_is_negative = false;
        switch (value[idx])
        {
        case '-':
            exponent_is_negative = true;
            // fallthrough
        case '+':
            ++idx;
            break;
        default:
            break;
        }

        const size_t exponent_start_idx = idx;
        while (idx < value.size() && is_decimal_digit(value[idx]))
        {
            ++idx;
        }

        if (idx > exponent_start_idx)
        {
            std::string_view exp_digits = value.substr(exponent_start_idx, idx - exponent_start_idx);
            if (string_to_number_decimal(exp_digits, out_result.exponent))
            {
                if (exponent_is_negative)
                {
                    out_result.exponent = -out_result.exponent;
                }
            }
            else
            {
                out_error = ErrorInfo{ jxc::format("Failed to parse exponent '{}'", exp_digits), number_token.start_idx, number_token.end_idx };
                return false;
            }
        }
        else
        {
            out_error = ErrorInfo{ jxc::format("Number token '{}' has 'e' or 'E' but no exponent value", value), number_token.start_idx, number_token.end_idx };
            return false;
        }
    }

    // the lexer shouldn't ever give us a number type with no digits
    JXC_ASSERT(value_len > 0);

    out_result.value = value.substr(0, value_len);
    out_result.suffix = value.substr(idx);

    // if the suffix starts with an underscore, strip it (it's not part of the suffix)
    if (out_result.suffix.size() > 0 && out_result.suffix[0] == '_')
    {
        // "_" is not a valid suffix - it must contain characters after that
        if (out_result.suffix.size() == 1)
        {
            out_error = ErrorInfo{
                jxc::format("Number token '{}' has an invalid suffix (it has no characters after the underscore)", value),
                number_token.start_idx,
                number_token.end_idx
            };
            return false;
        }

        out_result.suffix = out_result.suffix.substr(1);
    }

    return true;
}


bool string_token_to_value(const Token& string_token, std::string_view& out_view, bool& out_is_raw_string, ErrorInfo& out_error)
{
    out_view = string_token.value.as_view();
    out_is_raw_string = detail::string_view_starts_with(out_view, raw_string_prefix);

    if (out_is_raw_string)
    {
        // strip `r` prefix
        out_view = out_view.substr(1);
    }

    // strip quotes
    if (detail::string_view_starts_with(out_view, '\'') || detail::string_view_starts_with(out_view, '\"'))
    {
        // this is a debug assert because the lexer should be giving us good data to begin with
        JXC_DEBUG_ASSERT(detail::string_view_ends_with(out_view, out_view.front()));
        out_view = out_view.substr(1, out_view.size() - 2);
    }
    else
    {
        out_error = ErrorInfo("Missing quotes", string_token.start_idx, string_token.end_idx);
        return false;
    }

    if (out_is_raw_string)
    {
        // strip heredoc if we have one
        std::string_view heredoc = string_token.tag.as_view();
        if (heredoc.size() > 0)
        {
            if (heredoc.size() > JXC_MAX_HEREDOC_LENGTH)
            {
                out_error = ErrorInfo(jxc::format("Invalid heredoc {} with length {} (max length is {})",
                    detail::debug_string_repr(heredoc), heredoc.size(), JXC_MAX_HEREDOC_LENGTH),
                    string_token.start_idx,
                    string_token.end_idx);
                return false;
            }

            if (detail::string_view_starts_with(out_view, heredoc))
            {
                out_view = out_view.substr(heredoc.size());
            }
            else
            {
                out_error = ErrorInfo(jxc::format("Expected raw string token to start with heredoc '{}'", heredoc),
                    string_token.start_idx, string_token.end_idx);
                return false;
            }

            if (detail::string_view_ends_with(out_view, heredoc))
            {
                out_view = out_view.substr(0, out_view.size() - heredoc.size());
            }
            else
            {
                out_error = ErrorInfo(jxc::format("Expected raw string token to end with heredoc '{}'", heredoc),
                    string_token.start_idx, string_token.end_idx);
                return false;
            }
        }

        // strip parens
        if (detail::string_view_starts_with(out_view, '('))
        {
            if (!detail::string_view_ends_with(out_view, ')'))
            {
                out_error = ErrorInfo(jxc::format("Raw string token has unmatched parentheses '{}'", heredoc),
                    string_token.start_idx, string_token.end_idx);
                return false;
            }
            out_view = out_view.substr(1, out_view.size() - 2);
        }
    }

    return true;
}


bool string_has_escape_chars(std::string_view value)
{
    for (size_t i = 0; i < value.size(); i++)
    {
        if (value[i] == '\\')
        {
            return true;
        }
    }
    return false;
}


size_t get_string_required_buffer_size(std::string_view value)
{
    if (value.size() <= 1)
    {
        return value.size();
    }

    size_t required_size = 0;
    size_t val_idx = 0;
    while (val_idx < value.size())
    {
        if (value[val_idx] == '\\')
        {
            // advance past backslash
            ++val_idx;

            // invalid escape sequence - we got a backslash at the end of the string
            if (val_idx >= value.size())
            {
                ++required_size;
                break;
            }

            // check the character after the backslash - this is the escape sequence type
            const uint8_t escape_seq_type = static_cast<uint8_t>(value[val_idx]);

            // skip past the escape sequence type character
            val_idx += 1;

            // This is tricky. This function can't return errors and should be very fast, so we're forced to overestimate
            // the required size at the cost of needing to allocate more memory for strings when parsing. In particular,
            // for unicode escapes, ideally we don't want to parse those escapes here, because then we'd have to handle
            // practically all unicode edge cases.
            // There are some good opportunities for optimization here.
            switch (escape_seq_type)
            {
            case '0':
            case 'a':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            case '\\':
            case '\'':
            case '\"':
                // single-character normal escape
                required_size += 1;
                break;
            case 'x':
                // escape in the form `\xFF` - always exactly one byte
                required_size += 1;
                val_idx += 2; // skip over 2 hex chars
                break;
            case 'u':
                // escape in the form `\uFFFF`
                required_size += 4; // assume worst-case scenario that this 16-bit escape requires 4 bytes after encoding
                val_idx += 4; // skip over 4 hex chars
                break;
            case 'U':
                // escape in the form `\UFFFFFFFF`
                required_size += 4; // assume worst-case scenario that this 32-bit escape requires 4 bytes after encoding
                val_idx += 8; // skip over 8 hex chars
                break;
            default:
                // invalid escape sequence
                required_size += 1;
                break;
            }
        }
        else
        {
            // Not an escape character. Just assume that one byte in the input equals one byte in the output.
            required_size += 1;
            val_idx += 1;
        }
    }

    return required_size;
}


bool get_string_token_required_buffer_size(const Token& string_token, size_t& out_buffer_size, ErrorInfo& out_error)
{
    std::string_view value;
    bool is_raw = false;
    if (!string_token_to_value(string_token, value, is_raw, out_error))
    {
        return false;
    }

    out_buffer_size = get_string_required_buffer_size(value, is_raw);
    return true;
}


bool parse_string_escapes_to_buffer(std::string_view value, size_t token_start_idx, size_t token_end_idx,
    char* out_string_buffer, size_t string_buffer_size, size_t& out_num_chars_written, ErrorInfo& out_error)
{
    out_num_chars_written = 0;
    if (value.size() == 0)
    {
        return true;
    }

    JXC_ASSERT(string_buffer_size > 0);

    auto write_char = [&](char ch)
    {
        out_string_buffer[out_num_chars_written] = ch;
        out_num_chars_written++;
    };

    if (value.size() == 1 && detail::is_renderable_ascii_char(value[0]))
    {
        write_char(value[0]);
        return true;
    }

    std::string deserialize_hex_error;
    std::string_view hex_sequence;

    size_t val_idx = 0;

    auto parse_hex_escape = [&](std::string_view string_value, size_t& inout_string_index, const char* hex_type, size_t req_num_hex_chars) -> bool
    {
        const size_t start_index = inout_string_index;
        std::string_view hex_seq = string_value.substr(inout_string_index, req_num_hex_chars);
        if (hex_seq.size() != req_num_hex_chars)
        {
            out_error = ErrorInfo(jxc::format("Truncated {} escape sequence", hex_type), token_start_idx, token_end_idx);
            return false;
        }

        inout_string_index += req_num_hex_chars;

        const uint32_t hex_cp = detail::deserialize_hex_to_codepoint(hex_seq.data(), hex_seq.size(), &deserialize_hex_error);
        if (hex_cp == 0 && deserialize_hex_error.size() > 0)
        {
            out_error = ErrorInfo(jxc::format("Failed deserialized hex characters for {} escape: {}",
                hex_type, deserialize_hex_error), token_start_idx, token_end_idx);
            return false;
        }

        const int32_t num_bytes_for_codepoint = detail::utf8::num_codepoint_bytes(hex_cp);
        if (out_num_chars_written + num_bytes_for_codepoint >= string_buffer_size)
        {
            out_error = ErrorInfo(jxc::format("Ran out of space while encoding {} codepoint `{}` in string at index {} (str={}, buf size={}, wrote num chars={})",
                hex_type, hex_seq, start_index, detail::debug_string_repr(string_value), string_buffer_size, out_num_chars_written), token_start_idx, token_end_idx);
            return false;
        }

#if JXC_DEBUG
        const int64_t start_num_chars = (int64_t)out_num_chars_written;
#endif

        detail::utf8::encode(out_string_buffer, string_buffer_size, out_num_chars_written, hex_cp);

#if JXC_DEBUG
        const int64_t num_utf_chars = (int64_t)out_num_chars_written - start_num_chars;
        JXC_DEBUG_ASSERT(num_utf_chars >= 1 && num_utf_chars <= 4);
        JXC_DEBUG_ASSERT(num_utf_chars == num_bytes_for_codepoint);
#endif

        return true;
    };

    while (val_idx < value.size())
    {
        if (value[val_idx] == '\\')
        {
            ++val_idx;
            if (val_idx >= value.size())
            {
                out_error = ErrorInfo("Strings can not end with a backslash", token_start_idx, token_end_idx);
                return false;
            }

            const uint32_t escape_seq_type = static_cast<uint32_t>(value[val_idx]);
            val_idx += 1;

            // check the escape sequence type
            switch (escape_seq_type)
            {
                // single-character normal escape
            case '0': write_char('\0'); break;
            case 'a': write_char('\a'); break;
            case 'b': write_char('\b'); break;
            case 'f': write_char('\f'); break;
            case 'n': write_char('\n'); break;
            case 'r': write_char('\r'); break;
            case 't': write_char('\t'); break;
            case 'v': write_char('\v'); break;
            case '\\': write_char('\\'); break;
            case '\'': write_char('\''); break;
            case '\"': write_char('\"'); break;
            case 'x':
                if (!parse_hex_escape(value, val_idx, "hex", 2))
                {
                    return false;
                }
                break;
            case 'u':
                if (!parse_hex_escape(value, val_idx, "utf16", 4))
                {
                    return false;
                }
                break;
            case 'U':
                if (!parse_hex_escape(value, val_idx, "utf32", 8))
                {
                    return false;
                }
                break;
            default:
                out_error = ErrorInfo("Invalid escape sequence", token_start_idx, token_end_idx);
                return false;
            }
        }
        else
        {
            write_char(value[val_idx]);
            ++val_idx;
        }
    }

    return true;
}


bool parse_string_token_to_buffer(const Token& string_token, char* out_buffer, size_t buffer_size, size_t& out_num_chars_written, ErrorInfo& out_error)
{
    out_num_chars_written = 0;

    std::string_view value;
    bool is_raw = false;
    if (!string_token_to_value(string_token, value, is_raw, out_error))
    {
        return false;
    }

    const size_t req_buf_size = get_string_required_buffer_size(value, is_raw);
    if (buffer_size < req_buf_size)
    {
        out_error = ErrorInfo(jxc::format("String parsing failed: required buffer of size {} but got buffer of size {}",
            req_buf_size, buffer_size), string_token.start_idx, string_token.end_idx);
        return false;
    }

    // string is not raw and has escape chars, so we need to parse the escape characters
    if (!is_raw && string_has_escape_chars(value))
    {
        return parse_string_escapes_to_buffer(value, string_token.start_idx, string_token.end_idx, out_buffer, buffer_size, out_num_chars_written, out_error);
    }

    // raw or no escape chars, we can just use memcpy and be done
    JXC_MEMCPY(out_buffer, buffer_size, value.data(), value.size());
    out_num_chars_written = value.size();
    return true;
}


size_t get_byte_buffer_required_size(const char* bytes_token_str, size_t bytes_token_str_len)
{
    std::string_view value = std::string_view{ bytes_token_str, bytes_token_str_len };
    if (value.size() >= 5 && detail::string_view_starts_with(value, base64_string_prefix))
    {
        if (value[4] == '(')
        {
            value = value.substr(5, value.size() - 7);
            return base64::get_num_bytes_in_base64_multiline_string(value.data(), value.size());
        }
        else
        {
            value = value.substr(4, value.size() - 5);
            return base64::get_num_bytes_in_base64_string(value.data(), value.size());
        }
    }

    return 0;
}


bool parse_bytes_token(const Token& bytes_token, uint8_t* out_data_buffer, size_t out_data_buffer_size, size_t& out_num_bytes_written, ErrorInfo& out_error)
{
    std::string_view value = bytes_token.value.as_view();
    JXC_ASSERT(value.size() >= 5);  // shortest possible byte string is `b64""`

    out_num_bytes_written = 0;

    if (detail::string_view_starts_with(value, base64_string_prefix))
    {
        // remove prefix
        value = value.substr(3);

        // remove quote chars
        if (value.size() >= 2 && (value[0] == '\'' || value[0] == '\"'))
        {
            value = value.substr(1, value.size() - 2);
        }
        else
        {
            out_error = ErrorInfo("Invalid base64 string", bytes_token.start_idx, bytes_token.end_idx);
            return false;
        }

        if (value.size() == 0)
        {
            return true;
        }

        size_t num_result_bytes = 0;

        if (value[0] == '(')
        {
            // multiline version (whitespace inside the string is allowed)
            if (value.size() < 2 || value.back() != ')')
            {
                out_error = ErrorInfo("Expected multiline base64 string to end with ')'", bytes_token.start_idx, bytes_token.end_idx);
                return false;
            }

            // strip inner parens
            value = value.substr(1, value.size() - 2);

            const size_t req_buffer_size = base64::get_num_bytes_in_base64_multiline_string(value.data(), value.size());
            if (out_data_buffer_size < req_buffer_size)
            {
                out_error = ErrorInfo("Output buffer too small while parsing bytes token", bytes_token.start_idx, bytes_token.end_idx);
                return false;
            }

            // string is empty or all whitespace
            if (req_buffer_size == 0)
            {
                return true;
            }

            num_result_bytes = base64::base64_multiline_to_bytes(value.data(), value.size(), out_data_buffer, req_buffer_size);
            if (num_result_bytes != req_buffer_size)
            {
                out_error = ErrorInfo("Failed parsing base64 string", bytes_token.start_idx, bytes_token.end_idx);
                return false;
            }

            out_num_bytes_written = num_result_bytes;
            return true;
        }
        else
        {
            // single line version (no whitespace allowed, much simpler to parse)
            const size_t req_buffer_size = base64::get_num_bytes_in_base64_string(value.data(), value.size());
            if (out_data_buffer_size < req_buffer_size)
            {
                out_error = ErrorInfo("Output buffer too small while parsing bytes token", bytes_token.start_idx, bytes_token.end_idx);
                return false;
            }

            base64::base64_to_bytes(value.data(), value.size(), out_data_buffer, out_data_buffer_size);
            out_num_bytes_written = req_buffer_size;
            return true;
        }
    }

    out_error = ErrorInfo("Invalid byte string", bytes_token.start_idx, bytes_token.end_idx);
    return false;
}


// Checks if a given token is of type DateTime and has the correct prefix and quote chars
static inline bool is_valid_datetime_token(const Token& datetime_token, std::string_view& out_value)
{
    out_value = datetime_token.value.as_view();

    if (datetime_token.type != TokenType::DateTime)
    {
        return false;
    }

    // require 'dt' prefix
    if (!detail::string_view_starts_with(out_value, "dt"))
    {
        return false;
    }

    // strip 'dt' prefix
    out_value = out_value.substr(2);

    // require quotes
    if (out_value.size() < 2 || out_value[0] != out_value[out_value.size() - 1] || (out_value[0] != '\'' && out_value[0] != '\"'))
    {
        return false;
    }

    // strip quotes
    out_value = out_value.substr(1, out_value.size() - 2);

    return true;
}


bool datetime_token_is_date(const Token& datetime_token)
{
    std::string_view value;
    return is_valid_datetime_token(datetime_token, value) && value.size() <= 16 && value.find_first_of('T') == std::string_view::npos;
}


bool datetime_token_is_datetime(const Token& datetime_token)
{
    std::string_view value;
    return is_valid_datetime_token(datetime_token, value) && value.find_first_of('T') != std::string_view::npos;
}


// datetime string components:
// Optional
//     '+' | '-' (pos/negative year)
// YYYYYY (year, 4-6 chars)
// '-'
// MM (month, zero padded, exactly 2 chars, values '01' through '12')
// '-'
// DD (day, zero padded, exactly 2 chars, values '01' through '31')
// can stop here for just a date value
// 'T' (indicates that we're starting a time value)
// HH (exactly 2 chars, values '00' through '24') [NB. "24:00:00" refers to the instant at the end of a calendar day]
// ':'
// MM (exactly 2 chars, values '00' through '59')
// Optional
//     ':'
//     SS (exactly 2 chars, values '00' through '59')
// Optional
//     '.'
//     ssssssssssss (decimal fraction of seconds, 1-12 digits)
// 'Z' | ('+HH:MM' | '-HH:MM')
//     If 'Z', then use UTC timezone.
//     If '+' or '-', then explicit timezone follows.
struct DateTimeTokenParser
{
    size_t tok_start_idx = 0;
    size_t tok_end_idx = 0;
    std::string_view value;
    size_t index = 0;
    bool done = false;

    DateTimeTokenParser(const Token& token, std::string_view value)
        : tok_start_idx(token.start_idx)
        , tok_end_idx(token.end_idx)
        , value(value)
    {}

    DateTimeTokenParser(const DateTimeTokenParser&) = delete;
    DateTimeTokenParser& operator=(const DateTimeTokenParser&) = delete;

    bool require_done(ErrorInfo& out_error, const char* error_msg = "")
    {
        JXC_ASSERTF(!done, "DateTimeTokenParser::require_done called more than once!");
        done = true;
        if (index < value.size())
        {
            std::string_view suffix = value.substr(index);
            if (error_msg[0] != '\0')
            {
                out_error = ErrorInfo(jxc::format("DateTime parse error: {} (invalid suffix {})", error_msg, detail::debug_string_repr(suffix)),
                    tok_start_idx, tok_end_idx);
            }
            else
            {
                out_error = ErrorInfo(jxc::format("DateTime parse error: invalid suffix {}", detail::debug_string_repr(suffix)),
                    tok_start_idx, tok_end_idx);
            }
            return false;
        }
        return true;
    }

    bool require_number_as_string(size_t min_digits, size_t max_digits, const char* number_type, std::string_view& out_value, ErrorInfo& out_error)
    {
        if (index + min_digits > value.size())
        {
            out_error = ErrorInfo(jxc::format("Invalid DateTime: Not enough characters remaining in token {} to parse {}",
                detail::debug_string_repr(value), number_type),
                tok_start_idx, tok_end_idx);
            return false;
        }

        size_t num_chars = 0;
        size_t new_index = index;
        while (new_index < value.size() && is_decimal_digit(value[new_index]))
        {
            ++num_chars;
            ++new_index;
        }

        if (num_chars < min_digits || num_chars > max_digits)
        {
            out_error = ErrorInfo(
                (min_digits == max_digits)
                    ? jxc::format("Invalid DateTime: Expected exactly {} digits for the {}, got {}", min_digits, number_type, num_chars)
                    : jxc::format("Invalid DateTime: Expected {}-{} digits for the {}, got {}", min_digits, max_digits, number_type, num_chars),
                tok_start_idx, tok_end_idx);
            return false;
        }

        out_value = value.substr(index, num_chars);
        JXC_ASSERT(out_value.size() == num_chars);
        JXC_ASSERT(out_value.size() >= min_digits && out_value.size() <= max_digits);
        index += out_value.size();

        return true;
    }

    template<typename IntType>
    bool require_number(size_t min_digits, size_t max_digits, const char* number_type, IntType& out_value, ErrorInfo& out_error)
    {
        static_assert(std::is_integral_v<IntType>, "require_number requires an integer type");

        std::string_view number_str;
        if (!require_number_as_string(min_digits, max_digits, number_type, number_str, out_error))
        {
            return false;
        }

        JXC_ASSERT(number_str.size() >= min_digits && number_str.size() <= max_digits);

        // strip leading zeroes
        while (number_str.size() > 1 && number_str[0] == '0')
        {
            number_str = number_str.substr(1);
        }

        if (!string_to_number_decimal<IntType>(number_str, out_value))
        {
            out_error = ErrorInfo(jxc::format("Invalid DateTime: {} is not a valid {}", detail::debug_string_repr(number_str), number_type),
                tok_start_idx, tok_end_idx);
            return false;
        }

        return true;
    }

    bool require_char(char required_char, ErrorInfo& out_error)
    {
        if (index >= value.size())
        {
            out_error = ErrorInfo(jxc::format("Invalid DateTime: expected character {}, got end of string",
                detail::debug_char_repr(required_char)),
                tok_start_idx, tok_end_idx);
            return false;
        }
        else if (value[index] != required_char)
        {
            out_error = ErrorInfo(jxc::format("Invalid DateTime: expected character {}, got {}",
                detail::debug_char_repr(required_char), detail::debug_char_repr(required_char)),
                tok_start_idx, tok_end_idx);
            return false;
        }

        ++index;
        return true;
    }

    inline char peek_char() const
    {
        return (index < value.size()) ? value[index] : '\0';
    }

    inline void advance()
    {
        ++index;
    }

    inline int64_t chars_remaining_including_current() const
    {
        return static_cast<int64_t>(value.size()) - static_cast<int64_t>(index);
    }

    inline bool at_end() const
    {
        return index >= value.size();
    }
};


bool parse_date_token(const Token& datetime_token, Date& out_date, ErrorInfo& out_error)
{
    std::string_view value;
    if (!is_valid_datetime_token(datetime_token, value))
    {
        out_error = ErrorInfo("Invalid date token", datetime_token.start_idx, datetime_token.end_idx);
        return false;
    }

    DateTimeTokenParser parser(datetime_token, value);

    char year_sign = parser.peek_char();
    if (year_sign == '+' || year_sign == '-')
    {
        parser.advance();
    }
    else
    {
        year_sign = '\0';
    }

    if (!parser.require_number(4, 6, "year", out_date.year, out_error)) { return false; }
    if (!parser.require_char('-', out_error)) { return false; }
    if (!parser.require_number(2, 2, "month", out_date.month, out_error)) { return false; }
    if (!parser.require_char('-', out_error)) { return false; }
    if (!parser.require_number(2, 2, "day", out_date.day, out_error)) { return false; }
    if (!parser.require_done(out_error)) { return false; }

    if (year_sign == '-')
    {
        out_date.year = -out_date.year;
    }

    return true;
}


// Parses a date token into a DateTime value.
// If require_time_data is true, this will return an error if the token does not include time info.
// If require_time_data is false and the token does not include time data, out_datetime will have a time of 00:00:00Z.
bool parse_datetime_token(const Token& datetime_token, DateTime& out_datetime, ErrorInfo& out_error, bool require_time_data)
{
    std::string_view value;
    if (!is_valid_datetime_token(datetime_token, value))
    {
        out_error = ErrorInfo("Invalid date token", datetime_token.start_idx, datetime_token.end_idx);
        return false;
    }

    DateTimeTokenParser parser(datetime_token, value);

    // date component
    char year_sign = parser.peek_char();
    if (year_sign == '+' || year_sign == '-')
    {
        parser.advance();
    }
    else
    {
        year_sign = '\0';
    }

    if (!parser.require_number(4, 6, "year", out_datetime.year, out_error)) { return false; }
    if (!parser.require_char('-', out_error)) { return false; }
    if (!parser.require_number(2, 2, "month", out_datetime.month, out_error)) { return false; }
    if (!parser.require_char('-', out_error)) { return false; }
    if (!parser.require_number(2, 2, "day", out_datetime.day, out_error)) { return false; }

    if (year_sign == '-')
    {
        out_datetime.year = -out_datetime.year;
    }

    if (parser.at_end())
    {
        JXC_DEBUG_ASSERT(!require_time_data);
        // no time or timezone specified - explicitly zero out those values.
        out_datetime.hour = 0;
        out_datetime.minute = 0;
        out_datetime.second = 0;
        out_datetime.tz_hour = 0;
        out_datetime.tz_minute = 0;
        return true;
    }

    if (!parser.require_char('T', out_error)) { return false; }
    if (!parser.require_number(2, 2, "hour", out_datetime.hour, out_error)) { return false; }
    if (!parser.require_char(':', out_error)) { return false; }
    if (!parser.require_number(2, 2, "minute", out_datetime.minute, out_error)) { return false; }

    // seconds can be optional
    if (parser.peek_char() == ':')
    {
        parser.advance();
        if (!parser.require_number(2, 2, "second", out_datetime.second, out_error)) { return false; }
    }

    // if we have fractional seconds, parse those and convert the value to nanoseconds
    if (parser.peek_char() == '.')
    {
        parser.advance();

        std::string_view fractional_seconds_str;
        if (!parser.require_number_as_string(1, 12, "fractional seconds", fractional_seconds_str, out_error)) { return false; }

        // we need the number of digits _before_ stripping leading zeroes to determine how to convert the value to nanoseconds
        const int num_digits = fractional_seconds_str.size();

        // strip leading zeroes
        while (fractional_seconds_str.size() > 1 && fractional_seconds_str[0] == '0')
        {
            fractional_seconds_str = fractional_seconds_str.substr(1);
        }

        if (fractional_seconds_str.size() == 1 && fractional_seconds_str[0] == '0')
        {
            // fast path for zero
            out_datetime.nanosecond = 0;
        }
        else
        {
            // convert to integer
            int64_t fractional_seconds = 0;
            if (!string_to_number_decimal(fractional_seconds_str, fractional_seconds))
            {
                out_error = ErrorInfo(jxc::format("Failed to convert fractional seconds value {} to number", detail::debug_string_repr(fractional_seconds_str)),
                    datetime_token.start_idx, datetime_token.end_idx);
                return false;
            }

            if (num_digits < 9)
            {
                // convert to nanoseconds (lossless)
                int64_t multiplier = 1;
                for (int64_t i = num_digits; i < 9; ++i)
                {
                    multiplier *= 10;
                }
                out_datetime.nanosecond = fractional_seconds * multiplier;
            }
            else if (num_digits > 9)
            {
                // convert to nanoseconds (lossy)
                int64_t divisor = 1;
                for (int64_t i = num_digits; i >= 9; --i)
                {
                    divisor *= 10;
                }
                out_datetime.nanosecond = fractional_seconds / divisor;
            }
            else
            {
                // value is already nanoseconds
                JXC_DEBUG_ASSERT(num_digits == 9);
                out_datetime.nanosecond = static_cast<uint32_t>(fractional_seconds);
            }
        }
    }

    // do we have timezone info?
    if (parser.at_end())
    {
        // no timezone - datetime is local
        out_datetime.tz_hour = 0;
        out_datetime.tz_minute = 0;
        out_datetime.tz_local = 1;
        return true;
    }

    const char tz_prefix = parser.peek_char();
    parser.advance();

    int8_t tz_minute_offset = 0;

    switch (tz_prefix)
    {
    case 'Z':
        // explicit UTC time indicator
        out_datetime.tz_hour = 0;
        out_datetime.tz_minute = 0;
        out_datetime.tz_local = 0;
        return parser.require_done(out_error, "timezone specified with 'Z', but it is not the last character");

    case '+':
    case '-':
        out_datetime.tz_local = 0;
        if (!parser.require_number(2, 2, "timezone hour offset", out_datetime.tz_hour, out_error)) { return false; }
        if (!parser.require_char(':', out_error)) { return false; }
        if (!parser.require_number(2, 2, "timezone minute offset", tz_minute_offset, out_error)) { return false; }
        out_datetime.tz_minute = tz_minute_offset;
        if (tz_prefix == '-')
        {
            out_datetime.tz_hour = -out_datetime.tz_hour;
        }
        return parser.require_done(out_error);

    default:
        break;
    }

    out_error = ErrorInfo("Failed parsing DateTime: unknown error", datetime_token.start_idx, datetime_token.end_idx);
    return false;


#if 0

    // Require either 20 or 25 characters. Allow 10 chars only if require_time_data is false.
    if (value.size() != 25 && value.size() != 20 && (value.size() != 10 || require_time_data))
    {
        if (require_time_data)
        {
            out_error = ErrorInfo(jxc::format("Invalid DateTime token: expected datetime value in the form {} or {}, got {}",
                date_format_ymd_hms, date_format_ymd_hms_tz, detail::debug_string_repr(value)),
                datetime_token.start_idx, datetime_token.end_idx);
        }
        else
        {
            out_error = ErrorInfo(jxc::format("Invalid DateTime token: expected datetime value in the form {}, {} or {}, got {}",
                date_format_ymd, date_format_ymd_hms, date_format_ymd_hms_tz, detail::debug_string_repr(value)),
                datetime_token.start_idx, datetime_token.end_idx);
        }
        return false;
    }

    DateTimeTokenParser parser(datetime_token, value);

    // date component
    if (!parser.require_number(4, "year", out_datetime.year, out_error)) { return false; }
    if (!parser.require_char('-', out_error)) { return false; }
    if (!parser.require_number(2, "month", out_datetime.month, out_error)) { return false; }
    if (!parser.require_char('-', out_error)) { return false; }
    if (!parser.require_number(2, "day", out_datetime.day, out_error)) { return false; }
    
    if (parser.at_end())
    {
        JXC_DEBUG_ASSERT(!require_time_data);
        // no time or timezone specified - explicitly zero out those values.
        out_datetime.hour = 0;
        out_datetime.minute = 0;
        out_datetime.second = 0;
        out_datetime.tz_hour = 0;
        out_datetime.tz_minute = 0;
        return true;
    }

    if (!parser.require_char('T', out_error)) { return false; }
    if (!parser.require_number(2, "hour", out_datetime.hour, out_error)) { return false; }
    if (!parser.require_char(':', out_error)) { return false; }
    if (!parser.require_number(2, "minute", out_datetime.minute, out_error)) { return false; }
    if (!parser.require_char(':', out_error)) { return false; }
    if (!parser.require_number(2, "second", out_datetime.second, out_error)) { return false; }

    // Z suffix indicates UTC time zone
    const char tz_char = parser.peek_char();
    parser.advance();

    auto set_tz_error = [&]() -> bool
    {
        out_error = ErrorInfo(jxc::format("Invalid DateTime token: value must end with 'Z', or a timezone offset in the form \"+HH:MM\" or \"-HH:MM\" (got suffix {})",
            detail::debug_string_repr(value.substr(parser.index))),
            datetime_token.start_idx, datetime_token.end_idx);
        return false;
    };

    switch (tz_char)
    {
    case '-':
        if (parser.chars_remaining_including_current() != 5)
        {
            return set_tz_error();
        }
        JXC_DEBUG_ASSERT(value.size() == 25);
        if (!parser.require_number(2, "timezone hour offset", out_datetime.tz_hour, out_error)) { return false; }
        if (!parser.require_char(':', out_error)) { return false; }
        if (!parser.require_number(2, "timezone minute offset", out_datetime.tz_minute, out_error)) { return false; }
        if (!parser.require_done(out_error)) { return false; }

        // this is a negative time zone offset
        out_datetime.tz_hour = -out_datetime.tz_hour;
        return true;

    case '+':
        if (parser.chars_remaining_including_current() != 5)
        {
            return set_tz_error();
        }
        JXC_DEBUG_ASSERT(value.size() == 25);
        if (!parser.require_number(2, "timezone hour offset", out_datetime.tz_hour, out_error)) { return false; }
        if (!parser.require_char(':', out_error)) { return false; }
        if (!parser.require_number(2, "timezone minute offset", out_datetime.tz_minute, out_error)) { return false; }
        if (!parser.require_done(out_error)) { return false; }
        return true;

    case 'Z':
        if (!parser.require_done(out_error)) { return false; }
        JXC_DEBUG_ASSERT(value.size() == 20);
        out_datetime.tz_hour = 0;
        out_datetime.tz_minute = 0;
        return true;

    default:
        break;
    }

    return set_tz_error();

#endif
}

} // namespace util

} // namespace jxc
