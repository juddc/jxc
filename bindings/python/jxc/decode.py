import enum
import typing
import dataclasses

import _pyjxc
from _pyjxc import Token, TokenType, Element, ElementType, ExpressionParseMode, Parser, ErrorInfo, ClassConstructMode

from .util import find_class


class ParseError(ValueError):
    def __init__(self, msg: str, err_info: typing.Optional[ErrorInfo] = None):
        self.err_info = err_info
        super().__init__(msg)


    @classmethod
    def make(cls, msg: str, obj: typing.Union[None, Element, Token], buffer: str):
        if isinstance(obj, Token):
            start_idx, end_idx = obj.start_idx, obj.end_idx
        elif isinstance(obj, Element):
            start_idx, end_idx = obj.token.start_idx, obj.token.end_idx
        else:
            start_idx, end_idx = _pyjxc.invalid_idx, _pyjxc.invalid_idx
        err = ErrorInfo(msg, start_idx, end_idx)
        err.get_line_and_col_from_buffer(buffer)
        return cls(err.to_string(buffer), err_info=err)



def lex(value: str) -> typing.Iterable[Token]:
    """
    JXC lexer - iterates over raw JXC tokens.
    Useful for writing a fully custom parser, or even tokenizing syntax that is similar to JXC syntax.
    """
    lexer = _pyjxc.Lexer(value)
    tok: Token = lexer.next()
    while tok.type != TokenType.EndOfStream:
        if tok.type == TokenType.Invalid:
            err: ErrorInfo = lexer.get_error()
            err.get_line_and_col_from_buffer(value)
            raise ParseError(err.to_string(value), err_info=err)
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
            raise ParseError(f'Annotation parse error: {lexer.get_error_message()}')
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
        err = parser.get_error()
        err.get_line_and_col_from_buffer(value)
        raise ParseError(err.to_string(value), err)


TokenLike = typing.Union[None, str, TokenType, tuple[str, TokenType]]
ValueConstructorCallable = typing.Union[type, typing.Callable[[typing.Any], typing.Any]]
ValueConstructor = typing.Union[
    None,
    ValueConstructorCallable,
    tuple[ValueConstructorCallable, ClassConstructMode],
    tuple[ValueConstructorCallable, ExpressionParseMode],
    tuple[ValueConstructorCallable, ClassConstructMode, ExpressionParseMode]
]


def loads(buf: typing.Union[str, bytes], *,
        decode_inline=False,
        decode_enum=False,
        decode_dataclass=False,
        default_expr_parse_mode=ExpressionParseMode.ValueList,
        ignore_unknown_annotations=True,
        ignore_unknown_number_suffixes=True,
        annotation_hooks: typing.Optional[typing.Iterable[tuple[typing.Union[TokenLike, typing.Iterable[TokenLike]], ValueConstructor]]] = None,
        number_suffix_hooks: typing.Optional[typing.Iterable[tuple[str, ValueConstructor]]] = None):
    parser = Parser(buf,
        default_expr_parse_mode=default_expr_parse_mode,
        ignore_unknown_annotations=ignore_unknown_annotations,
        ignore_unknown_number_suffixes=ignore_unknown_number_suffixes)

    if annotation_hooks is not None:
        for anno, construct_func in annotation_hooks:
            if isinstance(anno, str) and not _pyjxc.is_valid_identifier(anno):
                # If the annotation is a string that isn't a plain identifier, it's not going to work, because
                # _pyjxc.Parser.set_annotation_constructor treats strings as single token.
                # Split the string into tokens first.
                # This has the (intended) side effect of ignoring whitespace in annotation tokens,
                # so the annotations "list<f32, 8>" and "list< f32 , 8 >" are equal.
                anno = list(lex_annotation(anno))
            parser.set_annotation_constructor(anno, construct_func)

    if number_suffix_hooks is not None:
        for suffix, construct_func in number_suffix_hooks:
            parser.set_number_suffix_constructor(suffix, construct_func)

    if decode_inline or decode_enum or decode_dataclass:
        def lookup_class(ele: Element) -> ValueConstructor:
            if anno := ele.annotation.source():
                if cls := find_class(anno):
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
