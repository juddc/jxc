"""
This example encodes style rules in JXC, and converts them to CSS.
It shows how to handle and store data for duplicate keys, parse simple
expressions, and handle numeric values with units.

[Note that we don't attempt to support the full range of CSS syntax - this
is just an example of storing and transforming CSS-like data.]
"""
import jxc
import typing
from enum import Enum
from dataclasses import dataclass


JXC_STYLESHEET_DATA = r"""
{
    html: {
        background.color: rgb(0, 255, 0)
        text.color: rgb(0, 0, 0)
        font.family: ['Noto Sans', 'Segoe UI', 'Helvetica', 'sans-serif']
        font.size: 17px
    }
    h1: {
        font.size: 1.8rem
        font.weight: 500
    }
    h2: {
        font.size: 1.5rem
        font.weight: 400
    }
    h3: {
        font.size: 1.3rem
        font.weight: 300
        background.color: rgba(52, 50, 60, 0.75)
    }
    p: {
        font.size: 1rem
    }
    a: {
        text.color: rgb(0, 255, 255)
    }
    # '*' indicates pseudo-class, so JXC "a.*visited" -> CSS "a:visited"
    a.*visited: {
        text.color: rgb(127, 127, 255)
    }
    # '$' indicates class, so JXC "$icon" -> CSS ".icon"
    $icon: {
        width: 2.5rem
        height: 2.5rem
    }
    code: {
        font.family: ['Fira Code', 'Cascadia Code', 'monospace']
        font.size: 1.1rem
    }
    # Combining selectors with '.' indicates multiple combined selectors, so JXC "p.$abc" -> CSS "p.abc"
    nav.$sidebar: {
        width: 25%
    }
    # NB. Duplicate keys are allowed and handled correctly, both for selectors and properties
    nav.$sidebar: {
        width: 22%
        max.width: 250px
    }
}
"""


class Unit(Enum):
    """ All valid unit types. Suffixes not in this enum will raise a ValueError exception. """
    Px = 'px'
    Rem = 'rem'
    Vmin = 'vmin'
    Pct = '%'


@dataclass
class Value:
    """ Stores a value+unit pair """
    value: int | float
    unit: Unit

    def __str__(self):
        return f'{self.value}{self.unit.value}'


@dataclass
class RGBAColor:
    """ Stores an RGB color and alpha channel """
    r: int
    g: int
    b: int
    a: float = 1.0

    def __str__(self) -> str:
        if self.a != 1.0:
            return f'rgba({self.r}, {self.g}, {self.b}, {self.a})'
        # no alpha channel, convert to HTML color
        return f'#{int(self.r):02X}{int(self.g):02X}{int(self.b):02X}'


class KeyValuePairList:
    """ Very simple dict replacement that allows duplicate keys and maintains order """
    def __init__(self):
        self.data = []

    def __setitem__(self, key, val):
        self.data.append((key, val))

    def __iter__(self):
        for key, val in self.data:
            yield key, val
    
    def __repr__(self):
        return repr(self.data)


class FontList:
    """ Simple string list that is serialized as a quoted list of strings """
    def __init__(self, fonts: typing.Optional[list[str]] = None):
        self.fonts: list[str] = fonts or []

    def __str__(self):
        return ', '.join(f"'{font}'" for font in self.fonts)


class Ruleset:
    """ Set of style rules that be assigned to a selector """
    def __init__(self):
        self.rules = []
    
    def append(self, key: str, value: typing.Any):
        if key == 'font.family':
            # Auto-wrap font lists, since they are only used for this one selector type.
            # We could require an annotation for this instead, but this option is less verbose for the user.
            value = FontList(value)
        self.rules.append((key, value))

    def __iter__(self):
        for prop_name, prop_value in self.rules:
            prop_name = '-'.join(prop_name.split('.'))
            yield prop_name, prop_value


class Stylesheet:
    def __init__(self):
        self.rules: list[tuple[str, Ruleset]] = []

    def add_selector(self, selector: str) -> Ruleset:
        """ Adds a selector and returns the empty ruleset for that selector """
        new_ruleset = Ruleset()
        self.rules.append((selector, new_ruleset))
        return new_ruleset

    @classmethod
    def parse(cls, data: str):
        def make_from_suffix(suffix: str):
            def make(value):
                return Value(value=value, unit=Unit(suffix))
            return make

        def parse_comma_separated_values_expression(ele: jxc.Element, expr: list):
            result = []
            i = 0
            while i < len(expr):
                result.append(expr[i])
                i += 1
                if i < len(expr) and expr[i] != ',':
                    raise jxc.decode.ParseError.make(f'Expected comma, got {expr[i]!r}', ele, data)
                i += 1
            return result

        def make_from_annotation(ele: jxc.Element):
            if len(ele.annotation) == 1 and ele.annotation[0].type == jxc.TokenType.Identifier:
                match ele.annotation[0].value:
                    case 'rgb' | 'rgba':
                        return lambda parsed_expr: RGBAColor(*parse_comma_separated_values_expression(ele, parsed_expr))

            # You don't have to raise ParseError, but doing so with the related element and original data
            # adds full context to the error message (line, col, and related snippet)
            raise jxc.decode.ParseError.make(f'Invalid annotation `{ele.annotation!s}`', ele, data)

        # parse the input
        parser = jxc.Parser(data)
        parser.set_custom_dict_type(KeyValuePairList)
        parser.set_find_construct_from_number_suffix_callback(make_from_suffix)
        parser.set_find_construct_from_annotation_callback(make_from_annotation)
        result = parser.parse()

        # construct a Stylesheet from the parsed data
        styles = cls()
        for selector, rules in result:
            ruleset: Ruleset = styles.add_selector(selector)
            for key, value in rules:
                ruleset.append(key, value)
        return styles

    def _encode_css_selector(self, value: str) -> str:
        """ Converts the selector syntax used for this example to CSS """
        result = []
        for part in value.split('.'):
            if part.startswith('$'):
                result.append('.' + part[1:])
            elif part.startswith('*'):
                result.append(':' + part[1:])
            else:
                result.append(part)
        return ''.join(result)

    def __iter__(self):
        for selector, ruleset in self.rules:
            yield self._encode_css_selector(selector), ruleset

    def to_css(self) -> str:
        result = []
        for selector, ruleset in self:
            result.append(f'{selector} ' + '{')
            for prop_name, prop_value in ruleset:
                result.append(f'\t{prop_name}: {prop_value!s};')
            result.append('}')
        return '\n'.join(result)


if __name__ == "__main__":
    stylesheet: Stylesheet = Stylesheet.parse(JXC_STYLESHEET_DATA)
    print(stylesheet.to_css())
