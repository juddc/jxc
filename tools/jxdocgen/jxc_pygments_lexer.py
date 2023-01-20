import re
import typing
import jxc
import _pyjxc
from pygments import token as pygtoken
import pygments.lexer


__all__ = ['JXCLexer']


def _jxc_token_type_to_pygments_token_type(tok_type: jxc.TokenType):
    match tok_type:
        case None | jxc.TokenType.LineBreak | jxc.TokenType.EndOfStream:
            return pygtoken.Whitespace
        case jxc.TokenType.Invalid:
            return pygtoken.Error
        case jxc.TokenType.Comment:
            return pygtoken.Comment.Single
        case jxc.TokenType.Identifier:
            return pygtoken.Name.Class
        case jxc.TokenType.ExclamationPoint:
            return pygtoken.Name.Decorator
        case jxc.TokenType.ObjectKeyIdentifier:
            return pygtoken.Name.Property
        case jxc.TokenType.True_ | jxc.TokenType.False_ | jxc.TokenType.Null:
            return pygtoken.Keyword.Constant
        case jxc.TokenType.Number:
            return pygtoken.Number
        case jxc.TokenType.String:
            return pygtoken.String
        case jxc.TokenType.ByteString:
            return pygtoken.String.Other
        case jxc.TokenType.Colon:
            return pygtoken.Punctuation
        case (jxc.TokenType.Equals
                | jxc.TokenType.QuestionMark
                | jxc.TokenType.AtSymbol
                | jxc.TokenType.Pipe
                | jxc.TokenType.Ampersand
                | jxc.TokenType.Percent
                | jxc.TokenType.Semicolon):
            return pygtoken.Punctuation
        case jxc.TokenType.ExpressionOperator:
            return pygtoken.Operator
        case jxc.TokenType.Asterisk:
            return pygtoken.Operator
        case jxc.TokenType.Comma:
            return pygtoken.Punctuation
        case jxc.TokenType.Period:
            return pygtoken.Text
        case jxc.TokenType.BraceOpen:
            return pygtoken.Text
        case jxc.TokenType.BraceClose:
            return pygtoken.Text
        case jxc.TokenType.SquareBracketOpen:
            return pygtoken.Text
        case jxc.TokenType.SquareBracketClose:
            return pygtoken.Text
        case jxc.TokenType.AngleBracketOpen:
            return pygtoken.Text
        case jxc.TokenType.AngleBracketClose:
            return pygtoken.Text
        case jxc.TokenType.ParenOpen:
            return pygtoken.Text
        case jxc.TokenType.ParenClose:
            return pygtoken.Text
        case _:
            raise ValueError(f'Unhandled token {tok_type}')



def _jxc_lex_with_whitespace(text: str):
    last_end_idx = 0

    for tok in jxc.decode.lex(text):
        # callback for the whitespace before this token
        if tok.start_idx >= 0 and tok.start_idx > last_end_idx:
            slice_len = tok.start_idx - last_end_idx - 1
            #print(slice_len, repr(text[last_end_idx : last_end_idx + slice_len + 1]), last_end_idx)
            yield None, last_end_idx + 1, pygtoken.Whitespace, text[last_end_idx : last_end_idx + slice_len + 1]

        # callback for this token
        yield tok, tok.start_idx, tok.type, tok.value

        last_end_idx = tok.end_idx



class JXCLexer(pygments.lexer.Lexer):
    """
    Pygments wrapper for JXC that relies on JXC's native lexer.
    While this is very fast, it's not ideal because the native lexer's error handling is built around parsing,
    not syntax highlighting, which means that it doesn't handle errors as gracefully as a purpose-built lexer.
    """
    name = 'JXC'
    url = ''
    aliases = ['jxc']
    filenames = ['*.jxc']
    mimetypes = ['application/jxc']

    @staticmethod
    def register():
        import pygments.lexers._mapping
        lexer_name = JXCLexer.__class__.__name__
        if lexer_name not in pygments.lexers._mapping.LEXERS:
            pygments.lexers._mapping.LEXERS[lexer_name] = (
                'jxdocgen.jxc_pygments_lexer',
                JXCLexer.name,
                JXCLexer.aliases,
                JXCLexer.filenames,
                JXCLexer.mimetypes)

    def _lex_raw_string(self, start_idx: int, tok_value: str, heredoc: str):
        # min heredoc string: "()" (4 chars)
        if (len(tok_value) < 4
                or tok_value[0] not in ('"', "'")
                or tok_value[-1] not in ('"', "'")):
            yield (start_idx, pygtoken.Error, tok_value)
            return

        # first quote char
        yield start_idx, pygtoken.String, tok_value[0]
        start_idx += 1
        tok_value = tok_value[1:]
        
        # first heredoc name (including open paren)
        first_heredoc = tok_value[:len(heredoc) + 1]
        assert first_heredoc[-1] == '('
        yield start_idx, pygtoken.Name.Tag, first_heredoc
        start_idx += len(first_heredoc)
        tok_value = tok_value[len(first_heredoc):]

        # string suffix (close heredoc, close paren, and end quote)
        suffix = tok_value[:-(1 + 1 + len(heredoc))]

        # string inner value
        str_value = tok_value[:len(suffix)]
        yield start_idx, pygtoken.String.Heredoc, str_value
        start_idx += len(str_value)
        tok_value = tok_value[len(str_value):]

        # second heredoc name
        yield start_idx, pygtoken.Name.Tag, tok_value[:len(heredoc) + 1]
        start_idx += len(heredoc) + 1
        tok_value = tok_value[-1]

        # last quote char
        assert len(tok_value) == 1
        yield start_idx, pygtoken.String, tok_value[0]


    def get_tokens_unprocessed(self, text: str) -> typing.Iterator[tuple[int, jxc.TokenType, str]]:
        for tok, start_idx, tok_type, tok_value in _jxc_lex_with_whitespace(text):
            if tok is None:
                assert not isinstance(tok_type, jxc.TokenType)
                yield start_idx, tok_type, tok_value
                continue

            assert isinstance(tok, jxc.Token)

            if tok.type in (jxc.TokenType.String, jxc.TokenType.ByteString) and len(tok_value) >= 3:
                tok_value = tok.value
                str_prefix = ''
                i = 0
                while i < len(tok_value) and tok_value[i] != '\'' and tok_value[i] != '\"':
                    i += 1
                assert tok_value[i] == '\'' or tok_value[i] == '\"'
                if i > 0:
                    str_prefix = tok_value[0:i]
                    tok_value = tok_value[i:]
                    yield tok.start_idx, pygtoken.Name.Tag, str_prefix

                str_type = pygtoken.String
                if len(str_prefix) > 0:
                    if str_prefix == 'r' and len(tok_value) >= 4:
                        assert text[tok.start_idx + i] == tok_value[0]
                        yield from self._lex_raw_string(tok.start_idx + i, tok_value, tok.tag)
                        continue
                    elif str_prefix in ('bx', 'b64'):
                        str_type = pygtoken.String.Other

                yield tok.start_idx + i, str_type, tok_value

            elif tok.type == jxc.TokenType.Number:
                tok_value = tok.value
                sign, prefix, number, exponent, suffix = _pyjxc.split_number_token_value(tok)
                #print(tok.type, f'({len(tok_value)}) "{tok_value}"', tok.start_idx, tok.end_idx, (sign, prefix, number, exponent, suffix))

                if len(suffix) > 0:
                    tok_value = tok_value[0: len(tok_value) - len(suffix)]
                
                match prefix:
                    case '0x':
                        yield tok.start_idx, pygtoken.Number.Hex, tok_value
                    case '0o':
                        yield tok.start_idx, pygtoken.Number.Oct, tok_value
                    case '0b':
                        yield tok.start_idx, pygtoken.Number.Bin, tok_value
                    case _:
                        if '.' in number or exponent < 0:
                            yield tok.start_idx, pygtoken.Number.Float, tok_value
                        else:
                            yield tok.start_idx, pygtoken.Number.Integer, tok_value

                if len(suffix) > 0:
                    yield tok.start_idx + len(tok_value), pygtoken.Name.Decorator, suffix

            else:
                yield tok.start_idx, _jxc_token_type_to_pygments_token_type(tok.type), tok.value

