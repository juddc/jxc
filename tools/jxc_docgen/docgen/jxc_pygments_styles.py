import pygments
import pygments.style
import pygments.formatters
from pygments import token as pygtoken


class OneMonokaiStyle(pygments.style.Style):
    """
    Based on https://github.com/azemoh/vscode-one-monokai
    """

    background_color = "#282c34"
    highlight_color = "#3E4451"

    line_height = 1.5
    line_number_color = "#495162"
    line_number_background_color = "#282c34"

    styles = {
        pygtoken.Token:                     "#f8f9fa", # class:  ''
        pygtoken.Whitespace:                "",        # class: 'w'
        pygtoken.Error:                     "#960050 bg:#1e0010", # class: 'err'
        pygtoken.Other:                     "",        # class 'x'

        pygtoken.Comment:                   "#676f7d", # class: 'c'
        pygtoken.Comment.Multiline:         "",        # class: 'cm'
        pygtoken.Comment.Preproc:           "#e06c75",        # class: 'cp'
        pygtoken.Comment.PreprocFile:       "#61afef",        # class: 'cpf'
        pygtoken.Comment.Single:            "",        # class: 'c1'
        pygtoken.Comment.Special:           "",        # class: 'cs'

        pygtoken.Keyword:                   "#e06c75", # class: 'k'
        pygtoken.Keyword.Constant:          "",        # class: 'kc'
        pygtoken.Keyword.Declaration:       "",        # class: 'kd'
        pygtoken.Keyword.Namespace:         "", # class: 'kn'
        pygtoken.Keyword.Pseudo:            "",        # class: 'kp'
        pygtoken.Keyword.Reserved:          "",        # class: 'kr'
        pygtoken.Keyword.Type:              "",        # class: 'kt'

        pygtoken.Operator:                  "#e06c75", # class: 'o'
        pygtoken.Operator.Word:             "",        # class: 'ow' - like keywords

        pygtoken.Punctuation:               "", # class: 'p'

        pygtoken.Name:                      "#f8f8f2", # class: 'n'
        pygtoken.Name.Attribute:            "", # class: 'na' - to be revised
        pygtoken.Name.Builtin:              "#98c379",        # class: 'nb'
        pygtoken.Name.Builtin.Pseudo:       "",        # class: 'bp'
        pygtoken.Name.Class:                "#61afef", # class: 'nc' - to be revised
        pygtoken.Name.Constant:             "#56b6c2", # class: 'no' - to be revised
        pygtoken.Name.Decorator:            "#e06c75", # class: 'nd' - to be revised
        pygtoken.Name.Entity:               "#56b6c2",        # class: 'ni'
        pygtoken.Name.Exception:            "#61afef", # class: 'ne'
        pygtoken.Name.Function:             "#98c379", # class: 'nf'
        pygtoken.Name.Property:             "#56b6c2", # class: 'py'
        pygtoken.Name.Label:                "",        # class: 'nl'
        pygtoken.Name.Namespace:            "#61afef",        # class: 'nn' - to be revised
        pygtoken.Name.Other:                "", # class: 'nx'
        pygtoken.Name.Tag:                  "#61afef", # class: 'nt' - like a keyword
        pygtoken.Name.Variable:             "#abb2bf",        # class: 'nv' - to be revised
        pygtoken.Name.Variable.Class:       "#61afef",        # class: 'vc' - to be revised
        pygtoken.Name.Variable.Global:      "#61afef",        # class: 'vg' - to be revised
        pygtoken.Name.Variable.Instance:    "#61afef",        # class: 'vi' - to be revised

        pygtoken.Number:                    "#c678dd", # class: 'm'
        pygtoken.Number.Float:              "",        # class: 'mf'
        pygtoken.Number.Hex:                "",        # class: 'mh'
        pygtoken.Number.Integer:            "",        # class: 'mi'
        pygtoken.Number.Integer.Long:       "",        # class: 'il'
        pygtoken.Number.Oct:                "",        # class: 'mo'

        pygtoken.Literal:                   "", # class: 'l'
        pygtoken.Literal.Date:              "", # class: 'ld'

        pygtoken.String:                    "#e5c07b", # class: 's'
        pygtoken.String.Backtick:           "",        # class: 'sb'
        pygtoken.String.Char:               "",        # class: 'sc'
        pygtoken.String.Doc:                "",        # class: 'sd' - like a comment
        pygtoken.String.Double:             "",        # class: 's2'
        pygtoken.String.Escape:             "", # class: 'se'
        pygtoken.String.Heredoc:            "#e4d1ae",        # class: 'sh'
        pygtoken.String.Interpol:           "#c678dd",        # class: 'si'
        pygtoken.String.Other:              "#e5997b", # class: 'sx'
        pygtoken.String.Regex:              "",        # class: 'sr'
        pygtoken.String.Single:             "",        # class: 's1'
        pygtoken.String.Symbol:             "",        # class: 'ss'

        pygtoken.Generic:                   "",        # class: 'g'
        pygtoken.Generic.Error:             "#960050 bg:#1e0010", # class: 'gr'

        # markup
        pygtoken.Generic.Strong:            "bold",    # class: 'gs'
        pygtoken.Generic.Emph:              "italic",  # class: 'ge'
        pygtoken.Generic.Deleted:           "#e06c75", # class: 'gd',
        pygtoken.Generic.Inserted:          "#98c379", # class: 'gi'
        pygtoken.Generic.Output:            "#56b6c2", # class: 'go'
        pygtoken.Generic.Prompt:            "bold", # class: 'gp'
        pygtoken.Generic.Heading:           "bold #e06c75",        # class: 'gh'
        pygtoken.Generic.Subheading:        "#98c379", # class: 'gu'
    }



class AyuLightStyle(pygments.style.Style):
    """
    Based on https://github.com/ayu-theme/vscode-ayu
    """

    background_color = '#f8f9fa'
    highlight_color = '#6cbf43'

    line_number_color = "#8a9199"
    line_number_background_color = "#f8f9fa"

    styles = {
        pygtoken.Token:                     "", # class:  ''
        pygtoken.Whitespace:                "",        # class: 'w'
        pygtoken.Error:                     "#e65050", # class: 'err'
        pygtoken.Other:                     "",        # class 'x'

        pygtoken.Comment:                   "#787b80", # class: 'c'
        pygtoken.Comment.Multiline:         "italic",        # class: 'cm'
        pygtoken.Comment.Preproc:           "#fa8d3e",        # class: 'cp'
        pygtoken.Comment.PreprocFile:       "#86b300",        # class: 'cpf'
        pygtoken.Comment.Single:            "italic",        # class: 'c1'
        pygtoken.Comment.Special:           "",        # class: 'cs'

        pygtoken.Keyword:                   "#fa8d3e", # class: 'k'
        pygtoken.Keyword.Constant:          "",        # class: 'kc'
        pygtoken.Keyword.Declaration:       "",        # class: 'kd'
        pygtoken.Keyword.Namespace:         "", # class: 'kn'
        pygtoken.Keyword.Pseudo:            "",        # class: 'kp'
        pygtoken.Keyword.Reserved:          "",        # class: 'kr'
        pygtoken.Keyword.Type:              "",        # class: 'kt'

        pygtoken.Operator:                  "#ed9366", # class: 'o'
        pygtoken.Operator.Word:             "",        # class: 'ow' - like keywords

        pygtoken.Punctuation:               "#5c6166", # class: 'p'

        pygtoken.Name:                      "#5c6166", # class: 'n'
        pygtoken.Name.Attribute:            "", # class: 'na' - to be revised
        pygtoken.Name.Builtin:              "#f07171",        # class: 'nb'
        pygtoken.Name.Builtin.Pseudo:       "",        # class: 'bp'
        pygtoken.Name.Class:                "#61afef", # class: 'nc' - to be revised
        pygtoken.Name.Constant:             "#a37acc", # class: 'no' - to be revised
        pygtoken.Name.Decorator:            "#e6ba7e", # class: 'nd' - to be revised
        pygtoken.Name.Entity:               "#399ee6",        # class: 'ni'
        pygtoken.Name.Exception:            "", # class: 'ne'
        pygtoken.Name.Function:             "#f2ae49", # class: 'nf'
        pygtoken.Name.Property:             "", # class: 'py'
        pygtoken.Name.Label:                "",        # class: 'nl'
        pygtoken.Name.Namespace:            "#86b300",        # class: 'nn' - to be revised
        pygtoken.Name.Other:                "", # class: 'nx'
        pygtoken.Name.Tag:                  "#55b4d4", # class: 'nt' - like a keyword
        pygtoken.Name.Variable:             "",        # class: 'nv' - to be revised
        pygtoken.Name.Variable.Class:       "",        # class: 'vc' - to be revised
        pygtoken.Name.Variable.Global:      "",        # class: 'vg' - to be revised
        pygtoken.Name.Variable.Instance:    "#f07171",        # class: 'vi' - to be revised

        pygtoken.Number:                    "#a37acc", # class: 'm'
        pygtoken.Number.Float:              "",        # class: 'mf'
        pygtoken.Number.Integer:            "",        # class: 'mi'
        pygtoken.Number.Integer.Long:       "",        # class: 'il'
        pygtoken.Number.Hex:                "italic",        # class: 'mh'
        pygtoken.Number.Bin:                "italic",        # class: 'mo'
        pygtoken.Number.Oct:                "italic",        # class: 'mb'

        pygtoken.Literal:                   "", # class: 'l'
        pygtoken.Literal.Date:              "", # class: 'ld'

        pygtoken.String:                    "#86b300", # class: 's'
        pygtoken.String.Backtick:           "",        # class: 'sb'
        pygtoken.String.Char:               "",        # class: 'sc'
        pygtoken.String.Doc:                "",        # class: 'sd' - like a comment
        pygtoken.String.Double:             "",        # class: 's2'
        pygtoken.String.Escape:             "", # class: 'se'
        pygtoken.String.Heredoc:            "",        # class: 'sh'
        pygtoken.String.Interpol:           "#fa8d3e",        # class: 'si'
        pygtoken.String.Other:              "", # class: 'sx'
        pygtoken.String.Regex:              "",        # class: 'sr'
        pygtoken.String.Single:             "",        # class: 's1'
        pygtoken.String.Symbol:             "",        # class: 'ss'

        pygtoken.Generic:                   "",        # class: 'g'
        pygtoken.Generic.Error:             "#e65050", # class: 'gr'

        # markup
        pygtoken.Generic.Strong:            "bold #f07171",    # class: 'gs'
        pygtoken.Generic.Emph:              "italic #f07171",  # class: 'ge'
        pygtoken.Generic.Deleted:           "#ff7383", # class: 'gd',
        pygtoken.Generic.Inserted:          "#478acc", # class: 'gi'
        pygtoken.Generic.Output:            "#5c6166", # class: 'go'
        pygtoken.Generic.Prompt:            "bold #4cbf99", # class: 'gp'
        pygtoken.Generic.Heading:           "bold #f07171",        # class: 'gh'
        pygtoken.Generic.Subheading:        "#f07171", # class: 'gu'
    }


CODE_STYLES = {
    'onemonokai': 'dark',
    #'monokai': 'dark',
    #'dracula': 'dark',
    # 'gruvbox': 'dark',
    #'nord': 'dark',
    # 'paraiso_dark': 'dark',
    # 'solarized_dark': 'dark',
    # 'zenburn': 'dark',

    'ayu': 'light',
    #'emacs': 'light',
    #'vs': 'light',
    # 'gruvbox_light': 'light',
    # 'paraiso_light': 'light',
    # 'solarized_light': 'light',
}


def get_pygments_theme_css(style: str, css_selector: str = '.code'):
    style_arg = None
    match style:
        case 'onemonokai':
            style_arg = OneMonokaiStyle
        case 'ayu':
            style_arg = AyuLightStyle
        case _:
            style_arg = style
    fmt = pygments.formatters.get_formatter_by_name('html', style=style_arg)
    return fmt.get_style_defs(css_selector)


def get_code_style_data(styles=None):
    if styles is None:
        styles = CODE_STYLES

    result: dict[str, tuple[str, str]] = {}
    for name, style_type in styles.items():
        result[name] = (style_type, get_pygments_theme_css(name, f'.code.{name}'))
    return result
