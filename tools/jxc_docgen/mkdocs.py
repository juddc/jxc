import os
import sys
import shutil
import typing
import argparse
from dataclasses import dataclass

import jinja2
import markdown
import xml.etree.ElementTree as etree
from markdown.treeprocessors import Treeprocessor
from markdown.extensions import Extension
import docgen.jxc_pygments_styles

import jxc.decode

import docgen.jxc_pygments_lexer
docgen.jxc_pygments_lexer.JXCLexer.register()

import docgen.diagrams

import docgen.parser


# monkey-patch markdown codehilite extension to add a language-LANG css class to code blocks
import markdown.extensions.codehilite
class CodeHiliteExt(markdown.extensions.codehilite.CodeHilite):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if isinstance(self.lang, str) and len(self.lang) > 0:
            if 'cssclass' not in self.options:
                self.options['cssclass'] = ''
            self.options['cssclass'] += f' language-{self.lang}'
markdown.extensions.codehilite.CodeHilite = CodeHiliteExt


repo_root_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
template_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'templates'))
static_data_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static'))
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



def render_markdown_table_of_contents(markdown_data: str) -> str:
    # bit of a hack here - render markdown with a table of contents, then dump everything
    # after the table of contents
    toc_marker = '[TOC]'
    toc_end = '[TOC_END]'
    result = markdown.markdown(
        f'{toc_marker}\n\n{toc_end}\n\n{markdown_data}',
        output_format='html5',
        extensions=['toc', 'fenced_code'],
        extension_configs={
            'toc': {
                'permalink': True,
                'toc_class': 'menu',
            }
        })
    return result[:result.index(toc_end)].strip()


def all_nodes(node: etree.Element, level: int = 0):
    yield node, level
    for child in node:
        yield from all_nodes(child, level + 1)


def markdown_to_html(markdown_data: str, code_css_class: str = 'code', add_section_links: bool = True) -> str:
    class HeaderAnchorProcessor(Treeprocessor):
        header_tags = set([ f'h{i}' for i in range(1, 7) ])
        def run(self, root: etree.Element):
            for node, _level in all_nodes(root):
                assert isinstance(node, etree.Element)
                if (node.tag in self.header_tags) and (header_id := node.attrib.get('id', None)):
                    # replace the id tag in headers with a named anchor (easier to offset with CSS
                    # to compensate for a static header)
                    anchor = etree.Element('a')
                    anchor.attrib['name'] = header_id
                    anchor.attrib['class'] = 'anchor'
                    del node.attrib['id']
                    node.insert(0, anchor)
            return super().run(root)

    class HeaderAnchorExtension(Extension):
        def extendMarkdown(self, md: markdown.Markdown):
            md.treeprocessors.register(HeaderAnchorProcessor(md), 'header_anchor_fix', 0.0)

    extensions = [HeaderAnchorExtension(), 'fenced_code', 'codehilite']
    extension_configs = {
        'codehilite': {
            'use_pygments': True,
            'css_class': code_css_class,
        },
    }

    if add_section_links:
        extensions.append('toc')
        extension_configs['toc'] = {
            'permalink': False,
        }

    return markdown.markdown(markdown_data,
        output_format='html5',
        extensions=extensions,
        extension_configs=extension_configs,
    )


def make_menu(pages: list[docgen.parser.BasePage]) -> list[tuple[str, str]]:
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
    builder.ctx['menu'] = make_menu(doc_meta.pages)

    # passthrough global template vars
    for key, val in doc_meta.global_template_vars.items():
        builder.ctx[key] = val

    for page in doc_meta.pages:
        if isinstance(page, docgen.parser.MarkdownPage):
            # read in the markdown page
            with open(os.path.join(docs_dir, page.source), 'r') as fp:
                markdown_source = fp.read()

            # write out the file
            builder.render_to_file(
                template=page.template.file,
                output=page.output,
                ctx=page.template.make_context(
                    page_title=page.title,
                    toc_html=render_markdown_table_of_contents(markdown_source),
                    markdown_html=markdown_to_html(
                        markdown_source,
                        code_css_class=f'code {doc_meta.default_code_style}'),
            ))

        elif isinstance(page, docgen.parser.JXCPage):
            # read in the JXC file
            with open(os.path.join(docs_dir, page.source), 'r') as fp:
                jxc_source = fp.read()

            # using JXC's lexer, parse out all the comments at the beginning of the file
            comments = []
            first_non_comment_index = 0
            for tok in jxc.decode.lex(jxc_source):
                match tok:
                    case jxc.Token(type=jxc.TokenType.Comment, value=value):
                        assert isinstance(value, str)
                        assert value.startswith('#')
                        comments.append(value[2:] if value.startswith('# ') else value[1:])
                    case jxc.Token(type=jxc.TokenType.LineBreak, value=value):
                        comments.append(value)
                    case _:
                        # save the first non-comment index so we can strip the comments
                        first_non_comment_index = tok.start_idx
                        break

            # remove the comments from the actual JXC source
            jxc_source = jxc_source[first_non_comment_index:]

            # convert to markdown so we get syntax highlighting
            jxc_source_as_markdown = f'```jxc\n{jxc_source}\n```'

            # write out the file
            builder.render_to_file(template=page.template.file, output=page.output, ctx=page.template.make_context(
                page_title=page.title,
                desc=markdown_to_html(
                    ''.join(comments),
                    add_section_links=False),
                markdown_html=markdown_to_html(
                    jxc_source_as_markdown,
                    code_css_class=f'code {doc_meta.default_code_style}',
                    add_section_links=False),
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

    # copy all static files
    static_file_dest = os.path.join(output_dir, 'static')
    os.makedirs(static_file_dest, exist_ok=True)
    for filename in doc_meta.static_files:
        static_file_path = os.path.join(static_data_dir, filename)
        if not os.path.exists(static_file_path):
            raise FileNotFoundError(static_file_path)
        shutil.copy2(src=static_file_path, dst=os.path.join(static_file_dest, filename))



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', '-o', type=str, default=default_output_dir, help='Documentation build output directory')
    args = parser.parse_args()

    build_docs(args.output)
