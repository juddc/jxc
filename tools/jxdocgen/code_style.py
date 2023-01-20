import typing
from enum import Enum
import pygments.formatters
import pygments.styles
from . import style_onemonokai, style_ayulight


class CodeStyleType(Enum):
    Dark = 'Dark'
    Light = 'Light'


CODE_STYLES: dict[str, CodeStyleType] = {
    # styles from jxdocgen
    'onemonokai': CodeStyleType.Dark,
    'ayu': CodeStyleType.Light,

    # styles from pygments
    'default': CodeStyleType.Light,
    'emacs': CodeStyleType.Light,
    'friendly': CodeStyleType.Light,
    'friendly_grayscale': CodeStyleType.Light,
    'colorful': CodeStyleType.Light,
    'autumn': CodeStyleType.Light,
    'murphy': CodeStyleType.Light,
    'manni': CodeStyleType.Light,
    'material': CodeStyleType.Dark,
    'monokai': CodeStyleType.Dark,
    'perldoc': CodeStyleType.Light,
    'pastie': CodeStyleType.Light,
    'borland': CodeStyleType.Light,
    'trac': CodeStyleType.Light,
    'native': CodeStyleType.Dark,
    'fruity': CodeStyleType.Dark,
    'bw': CodeStyleType.Light,
    'vim': CodeStyleType.Dark,
    'vs': CodeStyleType.Light,
    'tango': CodeStyleType.Light,
    'rrt': CodeStyleType.Dark,
    'xcode': CodeStyleType.Light,
    'igor': CodeStyleType.Light,
    'paraiso-light': CodeStyleType.Light,
    'paraiso-dark': CodeStyleType.Dark,
    'lovelace': CodeStyleType.Light,
    'algol': CodeStyleType.Light,
    'algol_nu': CodeStyleType.Light,
    'arduino': CodeStyleType.Light,
    'rainbow_dash': CodeStyleType.Light,
    'abap': CodeStyleType.Light,
    'solarized-dark': CodeStyleType.Dark,
    'solarized-light': CodeStyleType.Light,
    'sas': CodeStyleType.Light,
    'staroffice' : CodeStyleType.Light,
    'stata': CodeStyleType.Light,
    'stata-light': CodeStyleType.Light,
    'stata-dark': CodeStyleType.Dark,
    'inkpot': CodeStyleType.Light,
    'zenburn': CodeStyleType.Dark,
    'gruvbox-dark': CodeStyleType.Dark,
    'gruvbox-light': CodeStyleType.Light,
    'dracula': CodeStyleType.Dark,
    'one-dark': CodeStyleType.Dark,
    'lilypond' : CodeStyleType.Light,
    'nord': CodeStyleType.Dark,
    'nord-darker': CodeStyleType.Dark,
    'github-dark': CodeStyleType.Dark,
}


def is_valid_style(style_name: str) -> bool:
    if style_name in ('onemonokai', 'ayu'):
        return True
    return pygments.styles.get_style_by_name(style_name) is not None


def get_style_type(style_name: str) -> CodeStyleType:
    if not is_valid_style(style_name):
        raise ValueError(f'Unknown style {style_name}')

    if style_name in CODE_STYLES:
        return CODE_STYLES[style_name]
    if style_name.endswith('dark'):
        return CodeStyleType.Dark
    elif style_name.endswith('light'):
        return CodeStyleType.Light

    print(f'Warning: unknown style {style_name}')
    return CodeStyleType.Dark


def _get_pygments_theme_css(style: str, css_selector: str):
    style_arg = None
    match style:
        case 'onemonokai':
            style_arg = style_onemonokai.OneMonokaiStyle
        case 'ayu':
            style_arg = style_ayulight.AyuLightStyle
        case _:
            style_arg = style
    fmt = pygments.formatters.get_formatter_by_name('html', style=style_arg)
    return fmt.get_style_defs(css_selector)


def get_code_style_css_data(styles: typing.Optional[list[str]] = None, css_class_name: str = 'code') -> dict[str, tuple[CodeStyleType, str]]:
    if styles is None:
        styles = list(CODE_STYLES.keys())

    assert len(styles) > 0

    result = {}
    for style_name in styles:
        if not is_valid_style(style_name):
            raise ValueError(f'Invalid code style {style_name!r} (valid styles: {list(CODE_STYLES.keys())!r})')
        result[style_name] = (
            get_style_type(style_name),
            _get_pygments_theme_css(style_name, css_selector=f'.{css_class_name}.{style_name}'),
        )

    return result
