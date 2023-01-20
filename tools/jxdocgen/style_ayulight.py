import pygments.style
from pygments import token as pygtoken


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

