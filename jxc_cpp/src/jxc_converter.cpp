#include "jxc_cpp/jxc_converter_struct.h"


JXC_BEGIN_NAMESPACE(jxc)

JXC_BEGIN_NAMESPACE(detail)

OwnedTokenSpan cpp_type_to_annotation(std::string_view cpp_type)
{
    std::ostringstream ss;

    if (cpp_type.size() > 0)
    {
        int64_t angle_depth = 0;
        TokenType last_last_tok_type = TokenType::Invalid;
        TokenType last_tok_type = TokenType::Invalid;

        Token tok;
        ErrorInfo err;
        Lexer lexer(cpp_type.data(), cpp_type.size());
        while (lexer.next(tok, err))
        {
            switch (tok.type)
            {
            case TokenType::Identifier:
                if (last_tok_type == TokenType::Identifier)
                {
                    ss << ' ';
                }
                ss << tok.value.as_view();
                break;
            case TokenType::Colon:
                if (last_tok_type == TokenType::Colon)
                {
                    // replace double-colon with single dot, only if the token before the double-colon was an identifier
                    if (last_last_tok_type == TokenType::Identifier)
                    {
                        ss << ".";
                    }
                }
                break;
            case TokenType::AngleBracketOpen:
                ++angle_depth;
                ss << '<';
                break;
            case TokenType::AngleBracketClose:
                --angle_depth;
                ss << '>';
                break;
            case TokenType::Comma:
                if (angle_depth > 0)
                {
                    ss << ", ";
                }
                break;
            case TokenType::Period:
            case TokenType::Ampersand:
            case TokenType::Pipe:
            case TokenType::Caret:
            case TokenType::Plus:
            case TokenType::Minus:
            case TokenType::Asterisk:
            case TokenType::Slash:
            case TokenType::Tilde:
            case TokenType::Percent:
            case TokenType::ParenOpen:
            case TokenType::ParenClose:
            case TokenType::Equals:
            case TokenType::ExclamationPoint:
            case TokenType::True:
            case TokenType::False:
            case TokenType::Number:
                if (angle_depth > 0)
                {
                    ss << tok.value.as_view();
                }
                break;
            default:
                // skip over invalid tokens
                break;
            }

            last_last_tok_type = last_tok_type;
            last_tok_type = tok.type;
        }
    }

    const std::string result_str = ss.str();
    std::string err;
    auto result = OwnedTokenSpan::parse_annotation(result_str, &err);
    JXC_ASSERTF(result.has_value(), "Failed parsing annotation {} (from the C++ type {}): {}",
        jxc::detail::debug_string_repr(result_str),
        jxc::detail::debug_string_repr(cpp_type),
        err);
    return OwnedTokenSpan(std::move(*result));
}

JXC_END_NAMESPACE(detail)

JXC_BEGIN_NAMESPACE(conv)

std::string Parser::parse_token_as_string(const Token& token)
{
    std::string result;

    switch (token.type)
    {
    case TokenType::String:
        // if the token is a string, parse it first (this handles string escapes, raw strings, heredocs, etc.)
        if (!util::parse_string_token<std::string>(token, result, error))
        {
            throw parse_error("Failed to parse string", error);
        }
        break;

    case TokenType::Identifier:
        // if the token is an identifier, we can just return the value - no need to parse it
        result = std::string(token.value.as_view());
        break;

    default:
        throw parse_error(format("Expected string or identifier, got {}", token_type_to_string(token.type)), value());
    }

    return result;
}


std::string_view Parser::require_identifier_annotation(TokenSpan anno, std::initializer_list<std::string_view> expected_identifiers, bool annotation_optional) const
{
    // helper for error message formatting
    auto make_valid_anno_list = [&]() -> std::string
    {
        std::ostringstream ss;
        size_t i = 0;
        for (const auto& valid_anno : expected_identifiers)
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

    // no annotation supplied
    if (anno.size() == 0)
    {
        if (expected_identifiers.size() == 0 || annotation_optional)
        {
            // annotation was optional, and no annotation was specified
            return std::string_view("");
        }

        throw parse_error(jxc::format("Annotation required (valid annotations: {})", jxc::detail::debug_string_repr(anno.source()), make_valid_anno_list()), value());
    }

    const size_t ctx_start_idx = anno[0].start_idx;
    const size_t ctx_end_idx = anno[anno.size() - 1].end_idx;

    if (expected_identifiers.size() == 0 && anno.size() > 0)
    {
        // expected no annotation at all, but found one
        throw parse_error(jxc::format("Unexpected annotation {}", jxc::detail::debug_string_repr(anno.source())), ctx_start_idx, ctx_end_idx);
    }
    else if (anno.size() != 1)
    {
        // expected single-token annotation
        throw parse_error(jxc::format("Invalid annotation {} (valid annotations: {})", jxc::detail::debug_string_repr(anno.source()), make_valid_anno_list()), ctx_start_idx, ctx_end_idx);
    }

    // if the annotation we have is in the list of valid ones, return it
    for (const auto& valid_anno : expected_identifiers)
    {
        if (anno[0].value == valid_anno)
        {
            return anno[0].value.as_view();
        }
    }
    throw parse_error(jxc::format("Invalid annotation {} (valid annotations: {})", jxc::detail::debug_string_repr(anno.source()), make_valid_anno_list()), ctx_start_idx, ctx_end_idx);
}

JXC_END_NAMESPACE(conv)

JXC_END_NAMESPACE(jxc)
