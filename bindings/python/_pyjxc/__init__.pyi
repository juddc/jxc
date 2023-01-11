import typing
from typing import Callable
import enum

class AnnotationLexer:
    def __eq__(self, rhs: AnnotationLexer) -> bool: ...

    def __ge__(self, rhs: AnnotationLexer) -> bool: ...

    def __gt__(self, rhs: AnnotationLexer) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: AnnotationLexer, arg0: str): ...

    def __le__(self, rhs: AnnotationLexer) -> bool: ...

    def __lt__(self, rhs: AnnotationLexer) -> bool: ...

    def __ne__(self, rhs: AnnotationLexer) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    def get_error_message(self: AnnotationLexer) -> str: ...

    def has_error(self: AnnotationLexer) -> bool: ...

    def next(self: AnnotationLexer) -> Token: ...



class ClassConstructMode(enum.Enum):
    """
    Members:
      Auto : If the value is a list, pass values as `*args`. If the value is a dict,
    pass values as `** kwargs`. Otherwise pass a single value as the first argument
    to the class constructor.
      Value : Pass any value with the class's annotation as one argument to the
    class constructor
      ListAsArgs : Require that the class value is a list, and pass through all list
    values as `*args` to the class constructor
      DictAsKeywordArgs : Require that the class value is a dict, and pass through
    all dict key/value pairs as `**kwargs` to the class constructor
    """
    Auto = 0
    Value = 1
    ListAsArgs = 2
    DictAsKeywordArgs = 3


class Element:
    def __eq__(self: Element, arg0) -> bool: ...

    def __ge__(self, rhs: Element) -> bool: ...

    def __gt__(self, rhs: Element) -> bool: ...

    def __init__(self: Element, element_type: ElementType, token: Token, annotation: TokenSpan): ...

    def __le__(self, rhs: Element) -> bool: ...

    def __lt__(self, rhs: Element) -> bool: ...

    def __ne__(self: Element, arg0) -> bool: ...

    def __repr__(self: Element) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: Element) -> str: ...

    @property
    def annotation(self) -> TokenSpan: ...

    @property
    def token(self) -> Token: ...

    @token.setter
    def token(self, value: Token): ...

    @property
    def type(self) -> ElementType: ...

    @type.setter
    def type(self, value: ElementType): ...



class ElementType(enum.Enum):
    """
    Members:
      Invalid
      Number
      Bool
      Null
      Bytes
      String
      ExpressionIdentifier
      ExpressionOperator
      ExpressionToken
      Comment
      BeginArray
      EndArray
      BeginExpression
      EndExpression
      BeginObject
      ObjectKey
      EndObject
    """
    Invalid = 0
    Number = 1
    Bool = 2
    Null = 3
    Bytes = 4
    String = 5
    ExpressionIdentifier = 6
    ExpressionOperator = 7
    ExpressionToken = 8
    Comment = 9
    BeginArray = 10
    EndArray = 11
    BeginExpression = 12
    EndExpression = 13
    BeginObject = 14
    ObjectKey = 15
    EndObject = 16


class Encoder:
    def __eq__(self, rhs: Encoder) -> bool: ...

    def __ge__(self, rhs: Encoder) -> bool: ...

    def __gt__(self, rhs: Encoder) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: Encoder, settings: SerializerSettings, *, encode_inline: bool = False, sort_keys: bool = False, skip_keys: bool = False, decode_unicode: bool = True): ...

    def __le__(self, rhs: Encoder) -> bool: ...

    def __lt__(self, rhs: Encoder) -> bool: ...

    def __ne__(self, rhs: Encoder) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    @property
    def doc(self) -> Serializer: ...

    def encode_dict(self: Encoder, arg0: dict): ...

    def encode_sequence(self: Encoder, arg0: sequence): ...

    def encode_value(self: Encoder, arg0): ...

    def get_result(self: Encoder) -> str: ...

    def get_result_bytes(self: Encoder) -> bytes: ...

    def set_find_encoder_callback(self: Encoder, arg0): ...

    def set_find_fallback_encoder_callback(self: Encoder, arg0): ...

    def set_nan_encoder_callback(self: Encoder, arg0): ...



class ErrorInfo:
    def __bool__(self: ErrorInfo) -> bool: ...

    def __eq__(self, rhs: ErrorInfo) -> bool: ...

    def __ge__(self, rhs: ErrorInfo) -> bool: ...

    def __gt__(self, rhs: ErrorInfo) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(*args, **kwargs):
        """
        Overloaded function.
        1. __init__(self: _pyjxc.ErrorInfo) -> None
        2. __init__(self: _pyjxc.ErrorInfo, message: str, start_idx: int = invalid_idx,
        end_idx: int = invalid_idx) -> None
        """

    def __le__(self, rhs: ErrorInfo) -> bool: ...

    def __lt__(self, rhs: ErrorInfo) -> bool: ...

    def __ne__(self, rhs: ErrorInfo) -> bool: ...

    def __repr__(self: ErrorInfo) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: ErrorInfo) -> str: ...

    @property
    def buffer_end_idx(self) -> int: ...

    @buffer_end_idx.setter
    def buffer_end_idx(self, value: int): ...

    @property
    def buffer_start_idx(self) -> int: ...

    @buffer_start_idx.setter
    def buffer_start_idx(self, value: int): ...

    @property
    def col(self) -> int: ...

    @col.setter
    def col(self, value: int): ...

    def get_line_and_col_from_buffer(self: ErrorInfo, arg0: str) -> bool: ...

    @property
    def is_err(self) -> bool: ...

    @is_err.setter
    def is_err(self, value: bool): ...

    @property
    def line(self) -> int: ...

    @line.setter
    def line(self, value: int): ...

    @property
    def message(self) -> str: ...

    @message.setter
    def message(self, value: str): ...

    def reset(self: ErrorInfo): ...

    def to_string(self: ErrorInfo, buffer: str = '') -> str: ...



class ExpressionParseMode(enum.Enum):
    """
    Members:
      ValueList : Returns the expression as a list of values (excluding comments)
      ValueListWithComments : Returns the expression as a list of values (including
    comments)
      TokenList : Returns the expression as a list of Token objects
      SourceString : Returns the expression as a string, with the original
    formatting intact
    """
    ValueList = 0
    ValueListWithComments = 1
    TokenList = 2
    SourceString = 3


class ExpressionProxy:
    def __eq__(self, rhs: ExpressionProxy) -> bool: ...

    def __ge__(self, rhs: ExpressionProxy) -> bool: ...

    def __gt__(self, rhs: ExpressionProxy) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self, *args, **kwargs) -> typing.Any:
        """
        Initialize self.  See help(type(self)) for accurate signature.
        """

    def __le__(self, rhs: ExpressionProxy) -> bool: ...

    def __lt__(self, rhs: ExpressionProxy) -> bool: ...

    def __ne__(self, rhs: ExpressionProxy) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    def comma(self: ExpressionProxy) -> ExpressionProxy: ...

    def expression_end(self: ExpressionProxy) -> Serializer: ...

    def identifier(self: ExpressionProxy, arg0: str) -> ExpressionProxy: ...

    def identifier_or_string(self: ExpressionProxy, value: str, quote: StringQuoteMode = StringQuoteMode.Auto, decode_unicode: bool = True) -> ExpressionProxy: ...

    def op(self: ExpressionProxy, arg0: str) -> ExpressionProxy: ...

    def paren_close(self: ExpressionProxy) -> ExpressionProxy: ...

    def paren_open(self: ExpressionProxy) -> ExpressionProxy: ...

    def token(self: ExpressionProxy, arg0: TokenType) -> ExpressionProxy: ...

    def value_bool(self: ExpressionProxy, arg0: bool) -> ExpressionProxy: ...

    def value_bytes(self: ExpressionProxy, value: bytes, quote: StringQuoteMode = StringQuoteMode.Auto) -> ExpressionProxy: ...

    def value_bytes_base64(self: ExpressionProxy, value: bytes, quote: StringQuoteMode = StringQuoteMode.Auto) -> ExpressionProxy: ...

    def value_bytes_hex(self: ExpressionProxy, value: bytes, quote: StringQuoteMode = StringQuoteMode.Auto) -> ExpressionProxy: ...

    def value_float(self: ExpressionProxy, value: float, suffix: str = '', precision: int = 8) -> ExpressionProxy: ...

    def value_int(self: ExpressionProxy, value: int, suffix: str = '') -> ExpressionProxy: ...

    def value_int_bin(self: ExpressionProxy, value: int, suffix: str = '') -> ExpressionProxy: ...

    def value_int_hex(self: ExpressionProxy, value: int, suffix: str = '') -> ExpressionProxy: ...

    def value_int_oct(self: ExpressionProxy, value: int, suffix: str = '') -> ExpressionProxy: ...

    def value_null(self: ExpressionProxy) -> ExpressionProxy: ...

    def value_string(self: ExpressionProxy, value: str, quote: StringQuoteMode = StringQuoteMode.Auto, decode_unicode: bool = True) -> ExpressionProxy: ...

    def value_string_raw(self: ExpressionProxy, value: str, quote: StringQuoteMode = StringQuoteMode.Auto) -> ExpressionProxy: ...

    def write(self: ExpressionProxy, arg0: str) -> ExpressionProxy: ...



class FloatLiteralType(enum.Enum):
    """
    Members:
      Finite : Normal floating point value that is not nan, inf, or -inf
      NotANumber : Equivalent to float('nan')
      PosInfinity : Equivalent to float('inf')
      NegInfinity : Equivalent to float('-inf')
    """
    Finite = 0
    NotANumber = 1
    PosInfinity = 2
    NegInfinity = 3


class JumpParser:
    def __eq__(self, rhs: JumpParser) -> bool: ...

    def __ge__(self, rhs: JumpParser) -> bool: ...

    def __gt__(self, rhs: JumpParser) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: JumpParser, arg0: str): ...

    def __le__(self, rhs: JumpParser) -> bool: ...

    def __lt__(self, rhs: JumpParser) -> bool: ...

    def __ne__(self, rhs: JumpParser) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    def get_error(self: JumpParser) -> ErrorInfo: ...

    @staticmethod
    def get_profiler_results(sort_by_runtime: bool = True) -> str:
        """
        Requires compilation with JXC_ENABLE_JUMP_BLOCK_PROFILER=1
        """

    def has_error(self: JumpParser) -> bool: ...

    def next(self: JumpParser) -> bool: ...

    def reset(self: JumpParser, arg0: str): ...

    @staticmethod
    def reset_profiler():
        """
        Requires compilation with JXC_ENABLE_JUMP_BLOCK_PROFILER=1
        """

    def stack_depth(self: JumpParser) -> int: ...

    def value(self: JumpParser) -> Element: ...



class Lexer:
    def __eq__(self, rhs: Lexer) -> bool: ...

    def __ge__(self, rhs: Lexer) -> bool: ...

    def __gt__(self, rhs: Lexer) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: Lexer, arg0: str): ...

    def __le__(self, rhs: Lexer) -> bool: ...

    def __lt__(self, rhs: Lexer) -> bool: ...

    def __ne__(self, rhs: Lexer) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    def get_error(self: Lexer) -> ErrorInfo: ...

    def get_token_pos(self: Lexer) -> tuple: ...

    def next(self: Lexer) -> Token: ...



class LogLevel(enum.Enum):
    """
    Members:
      Info
      Warning
      Error
      Fatal
    """
    Info = 0
    Warning = 1
    Error = 2
    Fatal = 3


class OwnedElement:
    def __eq__(self: OwnedElement, arg0) -> bool: ...

    def __ge__(self, rhs: OwnedElement) -> bool: ...

    def __gt__(self, rhs: OwnedElement) -> bool: ...

    def __init__(self: OwnedElement, element_type: ElementType, token: Token, annotation: OwnedTokenSpan): ...

    def __le__(self, rhs: OwnedElement) -> bool: ...

    def __lt__(self, rhs: OwnedElement) -> bool: ...

    def __ne__(self: OwnedElement, arg0) -> bool: ...

    def __repr__(self: OwnedElement) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: OwnedElement) -> str: ...

    @property
    def annotation(self) -> OwnedTokenSpan: ...

    @property
    def token(self) -> Token: ...

    @token.setter
    def token(self, value: Token): ...

    @property
    def type(self) -> ElementType: ...

    @type.setter
    def type(self, value: ElementType): ...



class OwnedTokenSpan:
    def __bool__(self: OwnedTokenSpan) -> bool: ...

    def __eq__(self: OwnedTokenSpan, arg0: OwnedTokenSpan) -> bool: ...

    def __ge__(self, rhs: OwnedTokenSpan) -> bool: ...

    def __getitem__(self: OwnedTokenSpan, arg0: int) -> Token: ...

    def __gt__(self, rhs: OwnedTokenSpan) -> bool: ...

    def __init__(self, *args, **kwargs) -> typing.Any:
        """
        Initialize self.  See help(type(self)) for accurate signature.
        """

    def __le__(self, rhs: OwnedTokenSpan) -> bool: ...

    def __len__(self: OwnedTokenSpan) -> int: ...

    def __lt__(self, rhs: OwnedTokenSpan) -> bool: ...

    def __ne__(self: OwnedTokenSpan, arg0: OwnedTokenSpan) -> bool: ...

    def __repr__(self: OwnedTokenSpan) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: OwnedTokenSpan) -> str: ...

    def copy(self: OwnedTokenSpan) -> OwnedTokenSpan: ...

    def source(self: OwnedTokenSpan) -> str:
        """
        The original text from the buffer that represents all the tokens in this span.
        Primarily useful for accessing the original whitespace.
        """



class Parser:
    def __eq__(self, rhs: Parser) -> bool: ...

    def __ge__(self, rhs: Parser) -> bool: ...

    def __gt__(self, rhs: Parser) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: Parser, buf, *, default_expr_parse_mode: ExpressionParseMode = ExpressionParseMode.ValueList, ignore_unknown_annotations: bool = True, ignore_unknown_number_suffixes: bool = True): ...

    def __le__(self, rhs: Parser) -> bool: ...

    def __lt__(self, rhs: Parser) -> bool: ...

    def __ne__(self, rhs: Parser) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    @property
    def default_expr_parse_mode(self) -> ExpressionParseMode: ...

    @default_expr_parse_mode.setter
    def default_expr_parse_mode(self, value: ExpressionParseMode): ...

    def get_error(self: Parser) -> ErrorInfo: ...

    def has_error(self: Parser) -> bool: ...

    @property
    def ignore_unknown_annotations(self) -> bool: ...

    @ignore_unknown_annotations.setter
    def ignore_unknown_annotations(self, value: bool): ...

    @property
    def ignore_unknown_number_suffixes(self) -> bool: ...

    @ignore_unknown_number_suffixes.setter
    def ignore_unknown_number_suffixes(self, value: bool): ...

    def parse(self: Parser): ...

    def set_annotation_constructor(self: Parser, annotation, construct): ...

    def set_find_construct_from_annotation_callback(self: Parser, callback): ...

    def set_find_construct_from_number_suffix_callback(self: Parser, callback): ...

    def set_number_suffix_constructor(self: Parser, suffix: str, construct): ...



class Serializer:
    def __eq__(self, rhs: Serializer) -> bool: ...

    def __ge__(self, rhs: Serializer) -> bool: ...

    def __gt__(self, rhs: Serializer) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: Serializer, settings: SerializerSettings = SerializerSettings(pretty_print=true, encode_bytes_as_hexbytes=false, target_line_length=80, indent="    ", linebreak="\n", key_separator=": ", value_separator=",\n")): ...

    def __le__(self, rhs: Serializer) -> bool: ...

    def __lt__(self, rhs: Serializer) -> bool: ...

    def __ne__(self, rhs: Serializer) -> bool: ...

    def __repr__(self) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: Serializer) -> str: ...

    def annotation(self: Serializer, arg0: str) -> Serializer: ...

    def array_begin(self: Serializer, separator: str = '') -> Serializer: ...

    def array_empty(self: Serializer) -> Serializer: ...

    def array_end(self: Serializer) -> Serializer: ...

    def clear(self: Serializer): ...

    def comment(self: Serializer, arg0: str) -> Serializer: ...

    def done(self: Serializer): ...

    def expression_begin(self: Serializer) -> ExpressionProxy: ...

    def expression_empty(self: Serializer) -> Serializer: ...

    def expression_end(self: Serializer) -> Serializer: ...

    def flush(self: Serializer): ...

    def get_result(self: Serializer) -> str: ...

    def get_settings(self: Serializer) -> SerializerSettings: ...

    def identifier(self: Serializer, arg0: str) -> Serializer: ...

    def identifier_or_string(self: Serializer, value: str, quote: StringQuoteMode = StringQuoteMode.Auto, decode_unicode: bool = True) -> Serializer: ...

    def object_begin(self: Serializer, separator: str = '') -> Serializer: ...

    def object_empty(self: Serializer) -> Serializer: ...

    def object_end(self: Serializer) -> Serializer: ...

    def object_sep(self: Serializer) -> Serializer: ...

    def sep(self: Serializer) -> Serializer: ...

    def value_bool(self: Serializer, arg0: bool) -> Serializer: ...

    def value_bytes(self: Serializer, value: bytes, quote: StringQuoteMode = StringQuoteMode.Auto) -> Serializer: ...

    def value_bytes_base64(self: Serializer, value: bytes, quote: StringQuoteMode = StringQuoteMode.Auto) -> Serializer: ...

    def value_bytes_hex(self: Serializer, value: bytes, quote: StringQuoteMode = StringQuoteMode.Auto) -> Serializer: ...

    def value_float(self: Serializer, value: float, suffix: str = '', precision: int = 16, fixed: bool = False) -> Serializer: ...

    def value_int(self: Serializer, value: int, suffix: str = '') -> Serializer: ...

    def value_int_bin(self: Serializer, value: int, suffix: str = '') -> Serializer: ...

    def value_int_hex(self: Serializer, value: int, suffix: str = '') -> Serializer: ...

    def value_int_oct(self: Serializer, value: int, suffix: str = '') -> Serializer: ...

    def value_null(self: Serializer) -> Serializer: ...

    def value_string(self: Serializer, value: str, quote: StringQuoteMode = StringQuoteMode.Auto, decode_unicode: bool = True) -> Serializer: ...

    def value_string_raw(self: Serializer, value: str, quote: StringQuoteMode = StringQuoteMode.Auto, tag: str = '') -> Serializer: ...

    def write(self: Serializer, arg0: str) -> Serializer: ...



class SerializerSettings:
    def __eq__(self, rhs: SerializerSettings) -> bool: ...

    def __ge__(self, rhs: SerializerSettings) -> bool: ...

    def __gt__(self, rhs: SerializerSettings) -> bool: ...

    def __hash__(self) -> int: ...

    def __init__(self: SerializerSettings, *, pretty_print: bool = True, encode_bytes_as_hexbytes: bool = True, target_line_length: int = 80, indent: str = '    ', linebreak: str = '\n', key_separator: str = ': ', value_separator: str = ',\n', default_quote: StringQuoteMode = StringQuoteMode.Double, default_float_precision: int = 12, float_fixed_precision: bool = False): ...

    def __le__(self, rhs: SerializerSettings) -> bool: ...

    def __lt__(self, rhs: SerializerSettings) -> bool: ...

    def __ne__(self, rhs: SerializerSettings) -> bool: ...

    def __repr__(self: SerializerSettings) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self) -> str: ...

    @property
    def default_float_precision(self) -> int: ...

    @default_float_precision.setter
    def default_float_precision(self, value: int): ...

    @property
    def default_quote(self) -> StringQuoteMode: ...

    @default_quote.setter
    def default_quote(self, value: StringQuoteMode): ...

    @property
    def encode_bytes_as_hexbytes(self) -> bool: ...

    @encode_bytes_as_hexbytes.setter
    def encode_bytes_as_hexbytes(self, value: bool): ...

    @property
    def float_fixed_precision(self) -> bool: ...

    @float_fixed_precision.setter
    def float_fixed_precision(self, value: bool): ...

    def get_target_line_length(self: SerializerSettings) -> int: ...

    @property
    def indent(self) -> str: ...

    @indent.setter
    def indent(self, value: str): ...

    @property
    def key_separator(self) -> str: ...

    @key_separator.setter
    def key_separator(self, value: str): ...

    @property
    def linebreak(self) -> str: ...

    @linebreak.setter
    def linebreak(self, value: str): ...

    @staticmethod
    def make_compact() -> SerializerSettings: ...

    @property
    def pretty_print(self) -> bool: ...

    @pretty_print.setter
    def pretty_print(self, value: bool): ...

    @property
    def target_line_length(self) -> int: ...

    @target_line_length.setter
    def target_line_length(self, value: int): ...

    @property
    def value_separator(self) -> str: ...

    @value_separator.setter
    def value_separator(self, value: str): ...



class StringQuoteMode(enum.Enum):
    """
    Members:
      Auto
      Double
      Single
    """
    Auto = 0
    Double = 1
    Single = 2


class Token:
    def __eq__(self: Token, arg0: Token) -> bool: ...

    def __ge__(self, rhs: Token) -> bool: ...

    def __gt__(self, rhs: Token) -> bool: ...

    def __init__(self, *args, **kwargs) -> typing.Any:
        """
        Initialize self.  See help(type(self)) for accurate signature.
        """

    def __le__(self, rhs: Token) -> bool: ...

    def __lt__(self, rhs: Token) -> bool: ...

    def __ne__(self: Token, arg0: Token) -> bool: ...

    def __repr__(self: Token) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: Token) -> str: ...

    def copy(self: Token) -> Token: ...

    @property
    def end_idx(self) -> int: ...

    @end_idx.setter
    def end_idx(self, value: int): ...

    def get_line_and_col(self: Token, arg0: str) -> tuple: ...

    @property
    def start_idx(self) -> int: ...

    @start_idx.setter
    def start_idx(self, value: int): ...

    @property
    def tag(self) -> str: ...

    @tag.setter
    def tag(self, value: str): ...

    @property
    def type(self) -> TokenType: ...

    @type.setter
    def type(self, value: TokenType): ...

    @property
    def value(self) -> str: ...

    @value.setter
    def value(self, value: str): ...



class TokenSpan:
    def __bool__(self: TokenSpan) -> bool: ...

    def __eq__(self: TokenSpan, arg0: TokenSpan) -> bool: ...

    def __ge__(self, rhs: TokenSpan) -> bool: ...

    def __getitem__(self: TokenSpan, arg0: int) -> Token: ...

    def __gt__(self, rhs: TokenSpan) -> bool: ...

    def __init__(self, *args, **kwargs) -> typing.Any:
        """
        Initialize self.  See help(type(self)) for accurate signature.
        """

    def __le__(self, rhs: TokenSpan) -> bool: ...

    def __len__(self: TokenSpan) -> int: ...

    def __lt__(self, rhs: TokenSpan) -> bool: ...

    def __ne__(self: TokenSpan, arg0: TokenSpan) -> bool: ...

    def __repr__(self: TokenSpan) -> str: ...

    def __setattr__(self, name, value): ...

    def __str__(self: TokenSpan) -> str: ...

    def copy(self: TokenSpan) -> OwnedTokenSpan: ...

    def source(self: TokenSpan) -> str:
        """
        The original text from the buffer that represents all the tokens in this span.
        Primarily useful for accessing the original whitespace.
        """



class TokenType(enum.Enum):
    """
    Members:
      Invalid
      Comment
      Identifier
      ObjectKeyIdentifier
      True_
      False_
      Null
      Number
      String
      ByteString
      ExpressionOperator
      Colon
      Equals
      Comma
      Period
      BraceOpen
      BraceClose
      SquareBracketOpen
      SquareBracketClose
      AngleBracketOpen
      AngleBracketClose
      ParenOpen
      ParenClose
      ExclamationPoint
      Asterisk
      QuestionMark
      AtSymbol
      Pipe
      Ampersand
      Percent
      Semicolon
      LineBreak
      EndOfStream
    """
    Invalid = 0
    Comment = 1
    Identifier = 2
    ObjectKeyIdentifier = 3
    True_ = 4
    False_ = 5
    Null = 6
    Number = 7
    String = 8
    ByteString = 9
    ExpressionOperator = 10
    Colon = 11
    Equals = 12
    Comma = 13
    Period = 14
    BraceOpen = 15
    BraceClose = 16
    SquareBracketOpen = 17
    SquareBracketClose = 18
    AngleBracketOpen = 19
    AngleBracketClose = 20
    ParenOpen = 21
    ParenClose = 22
    ExclamationPoint = 23
    Asterisk = 24
    QuestionMark = 25
    AtSymbol = 26
    Pipe = 27
    Ampersand = 28
    Percent = 29
    Semicolon = 30
    LineBreak = 31
    EndOfStream = 32


def _jxc_assert(arg0: bool, arg1: str): ...

def debug_bytes_repr(value: bytes, quote_char: str = '"') -> str:
    """
    helper for creating a debug representation for bytes
    """

def debug_char_repr(codepoint, quote_char: str = '`') -> str:
    """
    helper for creating a debug representation for a single codepoint
    """

def debug_string_repr(value: str, quote_char: str = '"') -> str:
    """
    helper for creating a debug representation for a string
    """

def element_can_contain_annotation(arg0: ElementType) -> bool:
    """
    Returns true if a given element type is allowed to have an annotation
    """

def element_can_contain_value(arg0: ElementType) -> bool:
    """
    Returns true if a given element type can contain one or more value tokens. (eg.
    a Number element always contains a value token, but a Null element does not
    require one)
    """

def element_is_expression_value_type(arg0: ElementType) -> bool:
    """
    Returns true if a given element is valid for use inside an expression container
    """

def element_is_value_type(arg0: ElementType) -> bool:
    """
    Returns true if a given element is a valid value type (includes container-begin
    elements)
    """

invalid_idx: int = 18446744073709551615
def is_ascii_escape_char(codepoint, quote_char: str = '\x00') -> bool:
    """
    checks if a character should be escaped as ascii (eg. ' ')
    """

def is_debug_build() -> bool:
    """
    Returns True if pyjxc was compiled with JXC_DEBUG=1
    """

def is_profiler_enabled() -> bool:
    """
    Returns True if pyjxc was compiled with JXC_ENABLE_JUMP_BLOCK_PROFILER=1
    """

def is_renderable_ascii_char(codepoint) -> bool:
    """
    Checks if a given codepoint is a renderable ASCII char (eg. not a control
    character). NB: Returns true for space.
    """

def is_valid_identifier(arg0: str) -> bool: ...

def is_valid_identifier_char(arg0: str) -> bool: ...

def is_valid_identifier_first_char(arg0: str) -> bool: ...

def is_valid_object_key(arg0: str) -> bool: ...

def parse_bytes_token(arg0: Token): ...

def parse_number_token(number_token: Token) -> tuple:
    """
    Parses a number token to a float or int value. Always returns a 2-tuple in the
    form (value_or_errorinfo, number_suffix)
    """

def parse_string_token(arg0: Token): ...

def set_custom_assert_handler(arg0): ...

def set_custom_log_handler(arg0): ...

def split_number_token_value(arg0: Token):
    """
            Splits a number token into its component parts, and returns a tuple in
    the form (sign, prefix, number, exponent, suffix).
            All return types are strings except exponent, which is an int.
    """

def token_type_from_symbol(symbol: str) -> TokenType: ...

def token_type_has_value(arg0: TokenType) -> bool: ...

def token_type_to_symbol(arg0: TokenType) -> str: ...

