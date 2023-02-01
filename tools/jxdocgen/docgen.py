import os
import shutil
import typing

import jxc
import jinja2
from . import siteconfig, code_style, markdown_util, diagram_util


docgen_base_dir = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))
docgen_template_dir = os.path.join(docgen_base_dir, 'templates')
docgen_static_dir = os.path.join(docgen_base_dir, 'static')


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


def make_menu(pages: list[siteconfig.BasePage]) -> list[tuple[str, str]]:
    menu: list[tuple[str, str]] = []
    for page in pages:
        menu.append((page.output, page.title))
    return menu


def _jxc_page_to_html(docs_dir: str, site: siteconfig.SiteGenerator, page: siteconfig.JXCPage) -> tuple[str, str, str]:
    with open(os.path.join(docs_dir, page.source), 'r') as fp:
        jxc_source = fp.read()

    page_header_html = ''
    toc_html = ''

    if page.render_header_comments_as_markdown:
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
        
        # assume the comments at the top of the file is valid markdown
        page_header_markdown = ''.join(comments)

        # remove the comments from the actual JXC source
        jxc_source = jxc_source[first_non_comment_index:]

        # assemble the page header from the stripped comments
        md_exts, md_ext_cfg = siteconfig.make_markdown_context(site, page)
        page_header_html = markdown_util.markdown_to_html(page_header_markdown, md_exts, md_ext_cfg, site)

        # also generate a table of contents from the header markdown
        toc_html = markdown_util.render_markdown_table_of_contents(page_header_markdown)
    
    jxc_source_html = markdown_util.markdown_to_html(
        f'```jxc\n{jxc_source}\n```',
        extensions=['fenced_code', 'codehilite'],
        extension_configs={
            'codehilite': {
                'use_pygments': True,
                'css_class': 'code',
            }
        },
        site=site,
    )

    return (page_header_html, toc_html, jxc_source_html)


def get_template_dir(site: siteconfig.SiteGenerator) -> str:
    template_dir = site.template_dir
    if template_dir is None:
        template_dir = docgen_template_dir
    template_dir = os.path.abspath(template_dir)
    if not os.path.exists(template_dir):
        raise FileNotFoundError(f'{template_dir!r} is not a valid templates directory')
    return template_dir


def get_static_dir(site: siteconfig.SiteGenerator) -> str:
    static_dir = site.static_dir
    if static_dir is None:
        static_dir = docgen_static_dir
    static_dir = os.path.abspath(static_dir)
    if not os.path.exists(static_dir):
        raise FileNotFoundError(f'{static_dir!r} is not a valid static files directory')
    return static_dir


def build_docs(site_config_file: str, docs_dir: str, output_dir: str, dev_server: bool = False) -> siteconfig.SiteGenerator:
    # read in what docs pages to convert from docgen.jxc
    site_cfg: siteconfig.SiteGenerator = siteconfig.parse(site_config_file)

    template_dir = get_template_dir(site_cfg)
    static_dir = get_static_dir(site_cfg)

    if not site_cfg.default_code_style:
        site_cfg.default_code_style = list(code_style.CODE_STYLES.keys())[0]
    
    if site_cfg.default_code_style not in code_style.CODE_STYLES:
        raise ValueError(f'Invalid default code style {site_cfg.default_code_style} (styles = {list(code_style.CODE_STYLES.keys())!r})')

    # make a static dir for generated or copied static files
    static_file_dest = os.path.join(output_dir, 'static')
    os.makedirs(static_file_dest, exist_ok=True)

    builder = PageBuilder(template_dir, output_dir)
    builder.ctx['code_styles'] = code_style.get_code_style_css_data(
        styles=site_cfg.code_styles or ['onemonokai'],
        css_class_name=site_cfg.code_css_class or 'code')
    builder.ctx['diagram_styles'] = diagram_util.get_diagram_styles()
    builder.ctx['default_code_style'] = site_cfg.default_code_style
    builder.ctx['menu'] = make_menu(site_cfg.pages)
    builder.ctx['default_dark_or_light'] = code_style.get_style_type(site_cfg.default_code_style).name.lower()

    site_cfg.resolve_static_file_instances(static_dir, dev_server)

    # passthrough global template vars
    for key, val in site_cfg.global_template_vars.items():
        builder.ctx[key] = val

    for page in site_cfg.pages:
        if isinstance(page, siteconfig.MarkdownPage):
            # read in the markdown page
            with open(os.path.join(docs_dir, page.source), 'r') as fp:
                markdown_source = fp.read()
            
            md_exts, md_ext_cfg = siteconfig.make_markdown_context(site_cfg, page)

            # write out the file
            builder.render_to_file(
                template=page.template.file,
                output=page.output,
                ctx=siteconfig.make_template_context(site_cfg, page,
                    page_title=page.title,
                    toc_html=markdown_util.render_markdown_table_of_contents(markdown_source),
                    markdown_html=markdown_util.markdown_to_html(markdown_source, md_exts, md_ext_cfg, site_cfg),
                )
            )

        elif isinstance(page, siteconfig.JXCPage):
            # read in the JXC data and convert to html
            header_html, toc_html, body_html = _jxc_page_to_html(docs_dir, site_cfg, page)

            # write out the file
            builder.render_to_file(
                template=page.template.file,
                output=page.output,
                ctx=siteconfig.make_template_context(site_cfg, page,
                    page_title=page.title,
                    header_html=header_html,
                    toc_html=toc_html,
                    markdown_html=body_html,
            ))

        elif isinstance(page, siteconfig.RailroadDiagramsPage):
            # parse the diagrams from the JXC file
            diagrams = diagram_util.parse_syntax_defs(os.path.join(docs_dir, page.source))

            # write out the file
            builder.render_to_file(
                template=page.template.file,
                output=page.output,
                ctx=siteconfig.make_template_context(site_cfg, page,
                    page_title=page.title,
                    diagrams=[ (name, diagram_util.diagram_to_svg(diag)) for name, diag in diagrams ],
                )
            )
        else:
            raise TypeError(f'Unhandled page type {type(page).__name__}')

    for page in site_cfg.generated_static:
        if isinstance(page, siteconfig.GeneratedStaticPage):
            # write out the file
            builder.render_to_file(
                template=page.template.file,
                output=page.output,
                ctx=siteconfig.make_template_context(site_cfg, page,
                    page_title=page.title,
                )
            )
        else:
            raise TypeError(f'Unhandled static page type {type(page).__name__}')

    # copy all static files
    for path in site_cfg.all_static_files(static_dir):
        shutil.copy2(src=path, dst=os.path.join(static_file_dest, os.path.basename(path)))

    return site_cfg

