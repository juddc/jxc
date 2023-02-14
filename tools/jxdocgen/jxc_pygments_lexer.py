import re
import typing
import jxc
import _pyjxc
from pygments import token as pygtoken
import pygments.lexer


__all__ = ['JXCLexer']


def _jxc_token_type_to_pygments_token_type(tok_type: jxc.TokenType, tok_value: str = '', anno=False, anno_inner=False, expr=False):
    if expr and tok_type in (jxc.TokenType.QuestionMark, jxc.TokenType.Pipe, jxc.TokenType.Ampersand, jxc.TokenType.Percent,
            jxc.TokenType.Plus, jxc.TokenType.Minus, jxc.TokenType.Slash, jxc.TokenType.Caret, jxc.TokenType.Asterisk):
        return pygtoken.Operator
    elif expr and tok_type == jxc.TokenType.Identifier:
        match tok_value:
            # this isn't great for general-purpose use, but it works well for JXC's documentation
            case 'if' | 'else' | 'endif' | 'do' | 'while' | 'end' | 'local' | 'and' | 'or':
                return pygtoken.Keyword.Reserved
            case _:
                return pygtoken.Name.Variable
    elif anno or anno_inner:
        match tok_type:
            case jxc.TokenType.Identifier if anno_inner:
                return pygtoken.Name.Entity
            case jxc.TokenType.Identifier if not anno_inner:
                return pygtoken.Name.Class
            case jxc.TokenType.AngleBracketOpen | jxc.TokenType.AngleBracketClose | jxc.TokenType.Comma:
                return pygtoken.Punctuation.Marker
            case _:
                pass

    match tok_type:
        case None | jxc.TokenType.LineBreak | jxc.TokenType.EndOfStream:
            return pygtoken.Whitespace
        case jxc.TokenType.Invalid:
            return pygtoken.Error
        case jxc.TokenType.Comment:
            return pygtoken.Comment.Single
        case jxc.TokenType.Identifier:
            return pygtoken.Name.Property
        case jxc.TokenType.ExclamationPoint:
            return pygtoken.Name.Decorator
        case jxc.TokenType.True_ | jxc.TokenType.False_ | jxc.TokenType.Null:
            return pygtoken.Keyword.Constant
        case jxc.TokenType.Number:
            return pygtoken.Number
        case jxc.TokenType.String:
            return pygtoken.String
        case jxc.TokenType.ByteString:
            return pygtoken.String.Other
        case jxc.TokenType.DateTime:
            return pygtoken.Literal.Date
        case jxc.TokenType.Colon:
            return pygtoken.Punctuation
        case (jxc.TokenType.Equals
                | jxc.TokenType.QuestionMark
                | jxc.TokenType.AtSymbol
                | jxc.TokenType.Pipe
                | jxc.TokenType.Ampersand
                | jxc.TokenType.Percent
                | jxc.TokenType.Semicolon
                | jxc.TokenType.Plus
                | jxc.TokenType.Minus
                | jxc.TokenType.Slash
                | jxc.TokenType.Backslash
                | jxc.TokenType.Caret
                | jxc.TokenType.Tilde
                | jxc.TokenType.Backtick):
            return pygtoken.Punctuation
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



def _jxc_lex_with_whitespace(text: str, last_end_idx=0, index_offset=0):
    for tok in jxc.decode.lex(text):
        # callback for the whitespace before this token
        if tok.start_idx >= 0 and tok.start_idx > last_end_idx:
            slice_len = tok.start_idx - last_end_idx - 1
            #print(slice_len, repr(text[last_end_idx : last_end_idx + slice_len + 1]), last_end_idx)
            yield None, index_offset + last_end_idx + 1, pygtoken.Whitespace, text[last_end_idx : last_end_idx + slice_len + 1]

        # callback for this token
        yield tok, index_offset + tok.start_idx, _jxc_token_type_to_pygments_token_type(tok.type, tok.value), tok.value

        last_end_idx = tok.end_idx


def _whitespace_split(s: str) -> tuple[str, str, str]:
    """
    Splits a string into three strings in the form ("leading whitespace", "inner non whitespace", "trailing whitespace")
    """
    start = 0
    end = len(s) - 1
    while start < len(s) and s[start] in (' ', '\t', '\n', '\r'):
        start += 1
    while end >= start and s[end] in (' ', '\t', '\n', '\r'):
        end -= 1
    return s[:start], s[start:end+1], s[end+1:]


class JXCSyntaxHighlighter:
    def __init__(self, text: str):
        self._text = text
        self._last_end_index = 0
        self._parser = _pyjxc.JumpParser(self._text)
        self._current: typing.Optional[jxc.Element] = None
        self._in_expression = False

    def _advance(self):
        if self._parser.next():
            ele: jxc.Element = self._parser.value()
            if ele.type != jxc.ElementType.Invalid:
                self._current = ele
                return True
        if not self._parser.has_error():
            return False
        err = self._parser.get_error()
        err.get_line_and_col_from_buffer(self._text)
        raise jxc.ParseError(err.to_string(self._text), err)

    def _yield_token(self, tok: jxc.Token, anno=False, anno_inner=False):
        # callback for any text between the last token and this one
        if tok.start_idx >= 0 and tok.start_idx > self._last_end_index:
            slice_len = tok.start_idx - self._last_end_index - 1
            leading_str = self._text[self._last_end_index : self._last_end_index + slice_len + 1]
            #print(slice_len, repr(leading_str), self._last_end_index)
            idx = tok.start_idx
            ws_start, tok_value, ws_end = _whitespace_split(leading_str)
            if ws_start:
                yield None, idx, pygtoken.Whitespace, ws_start
                idx += len(ws_start)
            if tok_value:
                # If the leading text contained any non-whitespace that the parser skipped over (eg. comments),
                # lex that and include it here
                yield from _jxc_lex_with_whitespace(tok_value, last_end_idx=idx - 1, index_offset=idx)
                idx += len(tok_value)
            if ws_end:
                yield None, idx, pygtoken.Whitespace, ws_end
                idx += len(ws_end)

        # yield the requested token
        tok_type = _jxc_token_type_to_pygments_token_type(tok.type, tok.value, anno=anno, anno_inner=anno_inner, expr=self._in_expression)
        yield tok, tok.start_idx, tok_type, tok.value
        self._last_end_index = tok.end_idx

    def _parse_value(self):
        # yield annotation tokens - keep track of angle bracket depth so we can color tokens inside the angle brackets differently
        angle_bracket_level = 0
        for anno_tok in self._current.annotation:
            if anno_tok.type == jxc.TokenType.AngleBracketOpen:
                angle_bracket_level += 1
            elif anno_tok.type == jxc.TokenType.AngleBracketClose:
                angle_bracket_level -= 1
            yield from self._yield_token(anno_tok, anno=True, anno_inner=angle_bracket_level > 0)

        if self._current.type == jxc.ElementType.EndExpression:
            self._in_expression = False

        yield from self._yield_token(self._current.token)

        if self._current.type == jxc.ElementType.BeginExpression:
            self._in_expression = True

    def tokens(self):
        while self._advance():
            yield from self._parse_value()



def _jxc_parse_tokens_with_whitespace(text: str):
    try:
        yield from JXCSyntaxHighlighter(text).tokens()
    except jxc.ParseError as e:
        print(f"Failed parsing JXC snippet: {e}")
        for i, line in enumerate(text.split("\n")):
            print(f'[{i+1}] {line.rstrip()}')
        print("---")
        raise



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
        yield start_idx, pygtoken.Name.Other, first_heredoc
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
        yield start_idx, pygtoken.Name.Other, tok_value[:len(heredoc) + 1]
        start_idx += len(heredoc) + 1
        tok_value = tok_value[-1]

        # last quote char
        assert len(tok_value) == 1
        yield start_idx, pygtoken.String, tok_value[0]


    def _lex_datetime(self, start_idx: int, tok_value: str):
        assert len(tok_value) >= 2 and tok_value[0] == tok_value[-1] and tok_value[0] in ('\'', '\"')

        # first quote char
        yield start_idx, pygtoken.String, tok_value[0]
        tok_value = tok_value[1:]
        start_idx += 1

        # search for all "tokens" inside the datetime, which in this case is just defined as any number or single-character non-digit
        while len(tok_value) > 1:
            if num := re.match(r'([0-9]+)', tok_value):
                assert len(num.groups()) == 1
                num_value = num.groups()[0]
                yield start_idx, pygtoken.Number, num_value
                tok_value = tok_value[len(num_value):]
                start_idx += len(num_value)
            else:
                tok_type = pygtoken.Other
                if tok_value[0] in (':', '-'):
                    tok_type = pygtoken.Number
                elif tok_value[0] in ('T', 'Z'):
                    tok_type = pygtoken.Name.Decorator
                yield start_idx, tok_type, tok_value[0]
                tok_value = tok_value[1:]
                start_idx += 1

        # last quote char
        assert len(tok_value) == 1
        yield start_idx, pygtoken.String, tok_value


    def get_tokens_unprocessed(self, text: str) -> typing.Iterator[tuple[int, jxc.TokenType, str]]:
        for tok, start_idx, tok_type, tok_value in _jxc_parse_tokens_with_whitespace(text):
            if tok is None:
                assert not isinstance(tok_type, jxc.TokenType)
                yield start_idx, tok_type, tok_value
                continue

            assert isinstance(tok, jxc.Token)

            if tok.type in (jxc.TokenType.String, jxc.TokenType.ByteString, jxc.TokenType.DateTime) and len(tok_value) >= 3:
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
                    elif str_prefix == 'dt':
                        yield from self._lex_datetime(tok.start_idx + i, tok_value)
                        continue
                    elif str_prefix == 'b64':
                        str_type = pygtoken.String.Other

                yield tok.start_idx + i, str_type, tok_value

            elif tok.type == jxc.TokenType.Number:
                tok_value = tok.value
                sign, prefix, number, exponent, suffix, float_type = _pyjxc.split_number_token_value(tok)
                #print(tok.type, f'({len(tok_value)}) "{tok_value}"', tok.start_idx, tok.end_idx, (sign, prefix, number, exponent, suffix))

                if float_type != _pyjxc.FloatLiteralType.Finite:
                    yield tok.start_idx, pygtoken.Keyword.Constant, tok_value
                else:
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
                yield tok.start_idx, tok_type, tok.value

