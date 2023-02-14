import os
import sys
import re
import math
import enum
import typing
import dataclasses

from _pyjxc import Serializer, SerializerSettings, StringQuoteMode, Encoder, FloatLiteralType
from .util import get_class_path


EncodeFunc = typing.Callable[[Serializer, Encoder, typing.Any], None]


_OBJECT_KEY_IDENT_REGEX = re.compile(r'([a-zA-Z_$*][a-zA-Z0-9_$*]*)([\.\-][a-zA-Z_$*][a-zA-Z0-9_$*]*)*')
_EXPR_IDENT_REGEX = re.compile(r'[a-zA-Z_$][a-zA-Z0-9_$]*')


def is_valid_identifier_for_object_key(val: str) -> bool:
    """
    Returns True if the specified string is valid for use as an unquoted object key.
    """
    return _OBJECT_KEY_IDENT_REGEX.match(val) is not None



def is_valid_expression_identifier(val: str) -> bool:
    """
    Returns True if the specified string is valid for use as an unquoted identifer inside an expression.
    """
    return _EXPR_IDENT_REGEX.match(val) is not None



def enum_encoder(doc: Serializer, enc: Encoder, val: enum.Enum):
    doc.annotation(get_class_path(val.__class__))
    if isinstance(val, enum.IntEnum):
        doc.value_int(val.value)
    elif val.value is None:
        doc.value_null()
    elif isinstance(val.value, int):
        doc.value_int(val.value)
    elif isinstance(val.value, float):
        doc.value_float(val.value)
    elif isinstance(val.value, str):
        doc.value_string(val.value)
    else:
        raise TypeError(f"Failed to serialize enum type {type(val).__name__}: "
            "value type {type(val.value).__name__} is not supported")



def dataclass_encoder(doc: Serializer, enc: Encoder, val: typing.Any):
    doc.annotation(get_class_path(val.__class__))
    doc.object_begin()
    if hasattr(val.__class__, '__annotations__') and len(val.__class__.__annotations__) > 0:
        for var_name, var_type in val.__class__.__annotations__.items():
            if is_valid_identifier_for_object_key(var_name):
                doc.identifier(var_name)
            else:
                doc.value_string(var_name)
            doc.object_sep()
            enc.encode_value(getattr(val, var_name, None))
    else:
        for var_name, var_value in val.__dict__.items():
            if is_valid_identifier_for_object_key(var_name):
                doc.identifier(var_name)
            else:
                doc.value_string(var_name)
            doc.object_sep()
            enc.encode_value(var_value)
    doc.object_end()



def dumps(obj, *,
        indent: typing.Union[None, str, int] = None,
        sort_keys: bool = False,
        skip_keys: bool = False,
        decode_unicode: bool = False,
        check_circular: bool = True,
        separators: typing.Optional[tuple[str, str]] = None,
        encode_enum: bool = False,
        encode_dataclass: bool = False,
        encode_inline: bool = True,
        encode_class: typing.Optional[dict[type, EncodeFunc]] = None,
        default: typing.Optional[EncodeFunc] = None,
        quote: StringQuoteMode = StringQuoteMode.Double,
        as_bytes: bool = False) -> typing.Union[str, bytes]:
    """
    Simple JXC serializer with an API similar to json.dumps
    """
    if not indent:
        settings = SerializerSettings.make_compact()
    else:
        settings = SerializerSettings()
        settings.pretty_print = True
        if isinstance(indent, str):
            settings.indent = indent
        elif isinstance(indent, int):
            settings.indent = ' ' * indent
        else:
            raise TypeError(f'indent must be None, int, or str. Got {type(indent)}')

    settings.default_quote = quote

    if separators is not None:
        if not isinstance(separators, tuple) or len(separators) != 2:
            raise ValueError(f'separators should be a 2-tuple or None, got {separators!r}')
        settings.value_separator = separators[0]
        settings.key_separator = separators[1]
    
    #TODO: implement check_circular

    encoder = Encoder(
        settings,
        encode_inline=encode_inline,
        sort_keys=sort_keys,
        skip_keys=skip_keys,
        decode_unicode=decode_unicode)

    if encode_class is not None or encode_enum or encode_dataclass:
        def find_encoder(value: typing.Any) -> typing.Optional[typing.Callable[[Serializer, Encoder, typing.Any], None]]:
            if encode_inline and hasattr(value, '_jxc_encode'):
                # object has an inline encoder, so skip the enum/dataclass auto-encoding and let the class handle it
                return None
            elif encode_enum and isinstance(value, enum.Enum):
                return enum_encoder
            elif encode_dataclass and dataclasses.is_dataclass(value) and not isinstance(value, type):
                return dataclass_encoder
            elif encode_class is not None and (class_encoder := encode_class.get(type(value))):
                return class_encoder
            return None
        encoder.set_find_encoder_callback(find_encoder)

    if default is not None:
        def fallback_encoder(value: typing.Any):
            return default
        encoder.set_find_fallback_encoder_callback(fallback_encoder)

    encoder.encode_value(obj)
    return encoder.get_result_bytes() if as_bytes else encoder.get_result()


def dump(obj: typing.Any, fp: typing.TextIO, **kwargs):
    fp.write(dumps(obj, **kwargs))

