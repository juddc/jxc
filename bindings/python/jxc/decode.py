import enum
import typing
import dataclasses

import _pyjxc
from _pyjxc import (Token, TokenType, TokenList, Element, ElementType, ExpressionParseMode,
    Parser, ErrorInfo, ClassConstructMode)

from .util import find_class


class ParseError(ValueError):
    def __init__(self, msg: str, err_info: ErrorInfo, buf: str = ''):
        err_msg = f'{msg}: {err_info.message}' if msg else err_info.message
        self.err_info = ErrorInfo(err_msg, err_info.buffer_start_idx, err_info.buffer_end_idx)
        if buf:
            self.err_info.get_line_and_col_from_buffer(buf)
        super().__init__(self.err_info.to_string(buf))


    @classmethod
    def make(cls, msg: str, obj: typing.Union[None, Element, Token, TokenList], buf: str):
        if isinstance(obj, Token):
            start_idx, end_idx = obj.start_idx, obj.end_idx
        elif isinstance(obj, Element):
            start_idx, end_idx = obj.token.start_idx, obj.token.end_idx
        elif isinstance(obj, TokenList):
            if len(obj) > 0:
                start_idx, end_idx = obj[0].start_idx, obj[-1].end_idx
            else:
                start_idx, end_idx = _pyjxc.invalid_idx, _pyjxc.invalid_idx
        else:
            raise TypeError(f'Invalid ParseError context {obj!r}')
        return cls('', ErrorInfo(msg, start_idx, end_idx), buf)



def lex(value: str) -> typing.Iterable[Token]:
    """
    JXC lexer - iterates over raw JXC tokens.
    Useful for writing a fully custom parser, or even tokenizing syntax that is similar to JXC syntax.
    """
    lexer = _pyjxc.Lexer(value)
    tok: Token = lexer.next()
    while tok.type != TokenType.EndOfStream:
        if tok.type == TokenType.Invalid:
            raise ParseError('Lexer error', lexer.get_error(), value)
        yield tok
        tok = lexer.next()


def lex_annotation(value: str) -> typing.Iterable[Token]:
    """
    JXC lexer, specifically for lexing a string containing an annotation.
    """
    lexer = _pyjxc.AnnotationLexer(value)
    tok: Token = lexer.next()
    while tok.type != TokenType.EndOfStream:
        if tok.type == TokenType.Invalid:
            raise ParseError.make(f'Annotation parse error: {lexer.get_error_message()}', tok, value)
        yield tok
        tok = lexer.next()


def elements(value: str) -> typing.Iterable[Element]:
    """
    JXC parser - iterates over JXC elements.
    This is the core building block in writing a customized parser for custom data types.
    """
    parser = _pyjxc.JumpParser(value)
    while parser.next():
        ele: Element = parser.value()
        if ele.type == ElementType.Invalid:
            break
        else:
            yield ele

    if parser.has_error():
        raise ParseError('Parse error', parser.get_error(), value)


# types that can be converted into a token list
TokenLike = typing.Union[None, str, TokenType, typing.Tuple[str, TokenType], Token]

ValueConstructorCallable = typing.Union[type, typing.Callable[[typing.Any], typing.Any]]
ValueConstructor = typing.Union[
    None,
    ValueConstructorCallable,
    typing.Tuple[ValueConstructorCallable, ClassConstructMode],
    typing.Tuple[ValueConstructorCallable, ExpressionParseMode],
    typing.Tuple[ValueConstructorCallable, ClassConstructMode, ExpressionParseMode]
]


def loads(buf: typing.Union[str, bytes], *,
        decode_inline=False,
        decode_enum=False,
        decode_dataclass=False,
        default_expr_parse_mode=ExpressionParseMode.ValueList,
        ignore_unknown_annotations=True,
        ignore_unknown_number_suffixes=True,
        annotation_hooks: typing.Optional[typing.Dict[typing.Union[TokenLike, typing.Tuple[TokenLike], TokenList], ValueConstructor]] = None,
        number_suffix_hooks: typing.Optional[typing.Dict[str, ValueConstructor]] = None):
    parser = Parser(buf,
        default_expr_parse_mode=default_expr_parse_mode,
        ignore_unknown_annotations=ignore_unknown_annotations,
        ignore_unknown_number_suffixes=ignore_unknown_number_suffixes)

    if annotation_hooks:
        for anno, construct_func in annotation_hooks.items():
            #if isinstance(anno, str) and not _pyjxc.is_valid_identifier(anno):
                # If the annotation is a string that isn't a plain identifier, it's not going to work, because
                # _pyjxc.Parser.set_annotation_constructor treats strings as single token.
                # Split the string into tokens first.
                # This has the (intended) side effect of ignoring whitespace in annotation tokens,
                # so the annotations "list<f32, 8>" and "list< f32 , 8 >" are equal.
            #    anno = list(lex_annotation(anno))
            parser.set_annotation_constructor(anno, construct_func)

    if number_suffix_hooks is not None:
        for suffix, construct_func in number_suffix_hooks.items():
            parser.set_number_suffix_constructor(suffix, construct_func)

    if decode_inline or decode_enum or decode_dataclass:
        def lookup_class(anno: TokenList) -> ValueConstructor:
            if anno_src := anno.source():
                if cls := find_class(anno_src):
                    if decode_enum and issubclass(cls, enum.Enum):
                        return (cls, ClassConstructMode.Value)
                    elif decode_inline and hasattr(cls, '_jxc_decode'):
                        return cls
                    elif decode_dataclass and dataclasses.is_dataclass(cls):
                        return (cls, ClassConstructMode.DictAsKeywordArgs)
            return None
        parser.set_find_construct_from_annotation_callback(lookup_class)

    return parser.parse()


def load(fp: typing.TextIO, **kwargs) -> typing.Any:
    return loads(fp.read(), **kwargs)
