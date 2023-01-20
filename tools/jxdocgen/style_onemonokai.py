import pygments.style
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

