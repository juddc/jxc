import os
import sys
import typing
import argparse
from dataclasses import dataclass

import jinja2
import markdown
import docgen.jxc_pygments_styles

import docgen.jxc_pygments_lexer
docgen.jxc_pygments_lexer.JXCLexer.register()

import docgen.diagrams

import docgen.parser


repo_root_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
template_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'templates'))
default_output_dir = os.path.join(repo_root_dir, 'build', 'docs')


class PageBuilder:
    def __init__(self, template_dir: str, output_dir: str):
        self.env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dir))
        self.ctx = {}
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)

    def render_to_file(self, template: str, output: str, ctx: dict):
        with open(os.path.join(self.output_dir, output), 'w') as fp:
            context = { k: v for k, v in self.ctx.items() }
            for k, v in ctx.items():
                context[k] = v
            fp.write(self.env.get_template(template).render(context))



def all_markdown_docs(base_dir: str):
    for filename in os.listdir(base_dir):
        if not filename.endswith('.md'):
            continue

        page_name = filename[:-3]

        page_path = os.path.join(base_dir, filename)
        page_title = ''
        with open(page_path, 'r') as fp:
            for line in fp:
                if line.startswith('#'):
                    page_title = line
                    while len(page_title) > 0 and page_title.startswith('#'):
                        page_title = page_title[1:]
                    page_title = page_title.strip()
                    if len(page_title) > 0:
                        break
        
        if not page_title:
            page_title_parts = page_name.split('_')
            if page_title_parts[0] == 'jxc':
                page_title_parts.pop(0)
            page_title = ' '.join(part.capitalize() for part in page_title_parts)

        yield (page_name, page_title, page_path)



def markdown_to_html(markdown_data: str, code_css_class: str = 'code') -> str:
    return markdown.markdown(markdown_data,
        output_format='html5',
        extensions=['toc', 'fenced_code', 'codehilite'],
        extension_configs={
            'codehilite': {
                'css_class': code_css_class,
                'linenums': False,
            }
        })


def make_menu(docs_dir: str, pages: list[docgen.parser.BasePage]) -> list[tuple[str, str]]:
    menu: list[tuple[str, str]] = []
    for page in pages:
        menu.append((page.output, page.title))
    return menu


def build_docs(output_dir: str):
    docs_dir = os.path.join(repo_root_dir, 'docs')

    # read in what docs pages to convert from docgen.jxc
    doc_meta: docgen.parser.DocGen = docgen.parser.parse(os.path.join(docs_dir, 'docgen.jxc'))

    builder = PageBuilder(template_dir, output_dir)
    builder.ctx['code_styles'] = docgen.jxc_pygments_styles.get_code_style_data(styles=doc_meta.code_styles)
    builder.ctx['diagram_styles'] = docgen.diagrams.get_diagram_styles()
    builder.ctx['default_code_style'] = doc_meta.default_code_style
    builder.ctx['menu'] = make_menu(docs_dir, doc_meta.pages)

    # passthrough global template vars
    for key, val in doc_meta.global_template_vars.items():
        builder.ctx[key] = val

    for page in doc_meta.pages:
        if isinstance(page, docgen.parser.MarkdownPage):
            # read in the markdown page
            with open(os.path.join(docs_dir, page.source), 'r') as fp:
                markdown_source = fp.read()

            # write out the file
            builder.render_to_file(template=page.template.file, output=page.output, ctx=page.template.make_context(
                page_title=page.title,
                markdown_html=markdown_to_html(markdown_source, code_css_class=f'code {doc_meta.default_code_style}'),
            ))

        elif isinstance(page, docgen.parser.JXCPage):
            # read in the JXC file
            with open(os.path.join(docs_dir, page.source), 'r') as fp:
                jxc_source = fp.read()

            # convert to markdown so we get syntax highlighting
            jxc_source_as_markdown = f'```jxc\n{jxc_source}\n```'

            # write out the file
            builder.render_to_file(template=page.template.file, output=page.output, ctx=page.template.make_context(
                page_title=page.title,
                markdown_html=markdown_to_html(jxc_source_as_markdown, code_css_class=f'code {doc_meta.default_code_style}'),
            ))

        elif isinstance(page, docgen.parser.RailroadDiagramsPage):
            # parse the diagrams from the JXC file
            diagrams = docgen.diagrams.parse_syntax_defs(os.path.join(docs_dir, page.source))

            # write out the file
            builder.render_to_file(template=page.template.file, output=page.output, ctx=page.template.make_context(
                page_title=page.title,
                diagrams=[ (name, docgen.diagrams.diagram_to_svg(diag)) for name, diag in diagrams ],
            ))

        else:
            raise TypeError(f'Unhandled page type {type(page).__name__}')


    # # generate syntax diagrams
    # builder.ctx['menu'].append(('jxc_syntax', 'Syntax Tree'))
    # builder.ctx['menu'].append(('jxc_syntax_diagrams', 'Syntax Diagrams'))
    # builder.ctx['diagram_styles'] = docgen.diagrams.get_diagram_styles()
    # diagrams = docgen.diagrams.parse_syntax_defs(os.path.join(docs_dir, 'jxc_syntax.jxc'))

    # for page_name, page_title, path in all_markdown_docs(docs_dir):
    #     with open(path, 'r') as fp:
    #         builder.render_to_file(
    #             template='markdown_page.html',
    #             output=f'{page_name}.html',
    #             ctx={
    #                 'page_title': page_title,
    #                 'doc_html': markdown_to_html(fp.read(), code_css_class=f'code {default_code_style}'),
    #             })

    # builder.render_to_file(
    #     template='syntax_diagrams.html',
    #     output='jxc_syntax_diagrams.html',
    #     ctx={
    #         'page_title': 'Syntax Diagrams',
    #         'diagrams': [ (name, docgen.diagrams.diagram_to_svg(diag)) for name, diag in diagrams ],
    #     })



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', '-o', type=str, default=default_output_dir, help='Documentation build output directory')
    args = parser.parse_args()

    build_docs(args.output)
