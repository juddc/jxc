import jxc


def parse_html_template(buffer: str, template_context: dict, tokens: list[jxc.Token]):
    if len(tokens) < 3:
        return buffer[tokens[0].start_idx : tokens[-1].end_idx]

    last_part_start_idx = tokens[0].start_idx
    template_end_idx = tokens[-1].end_idx
    parts = []
    idx = 0
    while idx < len(tokens):
        if (idx + 3 < len(tokens)
                and tokens[idx].type == jxc.TokenType.ExpressionOperator and tokens[idx].value == '%'
                and tokens[idx + 1].type == jxc.TokenType.Identifier
                and tokens[idx + 2].type == jxc.TokenType.ExpressionOperator and tokens[idx + 2].value == '%'):
            parts.append(buffer[last_part_start_idx:tokens[idx].start_idx])
            var_name = tokens[idx + 1].value
            if template_value := template_context.get(var_name, None):
                parts.append(template_value)
            else:
                raise ValueError(f"Invalid template var {var_name!r}")
            idx += 3
            last_part_start_idx = tokens[idx].start_idx
        else:
            idx += 1

    # add in the part of the string between the last variable and the end of the expression
    if last_part_start_idx < template_end_idx:
        parts.append(buffer[last_part_start_idx:template_end_idx])

    return ''.join(parts)


if __name__ == "__main__":
    jxc_data = r"""
    {
        template: html(
            <!doctype html>
            <html>
            <body>
                <div class=%VAR1%>%DIV_CONTENTS%</div>
                <b id=%BOLD_ID%>%BOLD_VALUE%</b>
            </body>
            </html>
        )
    }
    """

    quoted = lambda s: f'"{s}"'
    template_vars = {
        'VAR1': quoted('div_class_name'),
        'DIV_CONTENTS': 'this is the inside of the div',
        'BOLD_ID': quoted('bold_please'),
        'BOLD_VALUE': "oh hai, I'm bold",
    }

    # parse the template
    result = jxc.loads(jxc_data,
        default_expr_parse_mode=jxc.ExpressionParseMode.TokenList,
        annotation_hooks=[
            ('html', lambda tokens: parse_html_template(jxc_data, template_vars, tokens)),
        ])

    print(result['template'])
