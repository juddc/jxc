import typing

try:
    import _pyjxc
except ImportError:
    import sys as _sys
    _sys.path.insert(0, '../build/debug')
    del _sys
    import _pyjxc

__version__ = _pyjxc.__version__

from _pyjxc import (
    # base
    ErrorInfo,

    # tokens
    Token, TokenType,

    # parsing helpers
    Parser, ExpressionParseMode, ClassConstructMode,
    token_type_has_value, token_type_to_symbol, split_number_token_value,

    # elements
    Element, ElementType,
    
    # element helpers
    element_can_contain_annotation, element_can_contain_value,
    element_is_expression_value_type, element_is_value_type,

    # serialization
    Serializer, SerializerSettings, Encoder, StringQuoteMode, FloatLiteralType,

    # constants
    invalid_idx,
)

from . import (encode, decode)

from .decode import (lex, elements, loads, ParseError)
from .encode import (dumps, )
from .util import (get_class_path, find_class)


from .decode import (load, loads, ParseError)
from .encode import (dump, dumps, EncodeFunc)

