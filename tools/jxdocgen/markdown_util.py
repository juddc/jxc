import typing
import xml.etree.ElementTree as etree
from markdown.treeprocessors import Treeprocessor
from markdown.extensions import Extension
import markdown.util
import markdown
from . import siteconfig


# monkey-patch pygments to enable JXC syntax highlighting
from . import jxc_pygments_lexer as _jxc_pygments_lexer
_jxc_pygments_lexer.JXCLexer.register()


# monkey-patch markdown codehilite extension to add a language-LANG css class to code blocks
import markdown.extensions.codehilite
class CodeHiliteExt(markdown.extensions.codehilite.CodeHilite):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if isinstance(self.lang, str) and len(self.lang) > 0:
            if 'cssclass' not in self.options:
                self.options['cssclass'] = ''
            csscls = self.lang.replace('c++', 'cpp')
            self.options['cssclass'] += f' language-{csscls}'
markdown.extensions.codehilite.CodeHilite = CodeHiliteExt



def _all_element_nodes(
        node: etree.Element,
        level: int = 0
        ) -> typing.Iterable[tuple[etree.Element, int]]:
    yield node, level
    for child in node:
        yield from _all_element_nodes(child, level + 1)



class HeaderAnchorProcessor(Treeprocessor):
    header_tags = set([ f'h{i}' for i in range(1, 7) ])
    def run(self, root: etree.Element):
        for node, _level in _all_element_nodes(root):
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
    _name = 'header_anchors'
    config = {}
    def extendMarkdown(self, md: markdown.Markdown):
        md.treeprocessors.register(HeaderAnchorProcessor(md), HeaderAnchorExtension._name, 0.0)
        md.registerExtension(self)



class PageLinksProcessor(Treeprocessor):
    def __init__(self, md: markdown.Markdown, config: typing.Optional[dict]):
        super().__init__(md)
        self.config = config or {}
        self.pages: dict[str, str] = {}
        if site := config.get('site', None):
            assert isinstance(site, siteconfig.SiteGenerator)
            for page in site.pages:
                self.pages[page.get_source_filename()] = page.output

    def run(self, root: etree.Element):
        ext_list: list[str] = ['md']
        if ext := self.config.get('extensions', None):
            if isinstance(ext, str):
                ext_list = [ext]
            elif isinstance(ext, (list, tuple)):
                ext_list = list(ext)
            else:
                raise TypeError

        for node, _level in _all_element_nodes(root):
            assert isinstance(node, etree.Element)
            if node.tag == 'a' and 'href' in node.attrib:
                target = node.attrib['href']
                if target in self.pages:
                    # we know the actual file path - use that
                    node.attrib['href'] = self.pages[target]
                else:
                    # just replace the extension with 'html'
                    for ext in ext_list:
                        if target.endswith(f'.{ext}'):
                            target = '.'.join(target.split('.')[:-1]) + ".html"
                            node.attrib['href'] = target
        return super().run(root)



class PageLinksExtension(Extension):
    _name = 'page_links'
    config = {
        'site': [None, 'SiteGenerator object'],
        'extensions': [['md'], 'List of file extensions to fix up'],
    }

    def __init__(self, **kwargs):
        self._site = kwargs.pop('site') if 'site' in kwargs else None
        super().__init__(**kwargs)

    def extendMarkdown(self, md: markdown.Markdown):
        md.treeprocessors.register(PageLinksProcessor(md, { **self.getConfigs(), 'site': self._site }), PageLinksExtension._name, 0.0)
        md.registerExtension(self)



class InlineCodeClassProcessor(Treeprocessor):
    def __init__(self, md: markdown.Markdown, config: typing.Optional[dict]) -> None:
        self.config = config or {}
        super().__init__(md)

    def run(self, root: etree.Element):
        css_class = self.config.get('css_class', None) or 'inline-code'
        for node, _level in _all_element_nodes(root):
            if node.tag == 'code':
                if 'class' in node.attrib:
                    node.attrib['class'] += f' {css_class}'
                else:
                    node.attrib['class'] = css_class
        return super().run(root)



class InlineCodeClassExtension(Extension):
    _name = 'inline_code_class'
    config = {
        'css_class': ['inline-code', 'CSS class to add to inline (non-fenced) code blocks'],
    }
    def extendMarkdown(self, md: markdown.Markdown):
        md.treeprocessors.register(InlineCodeClassProcessor(md, self.getConfigs()), InlineCodeClassExtension._name, 0.0)
        md.registerExtension(self)



def render_markdown_table_of_contents(markdown_data: str) -> str:
    """
    Renders just the Table of Contents portion of a markdown document
    """
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



def markdown_to_html(
        markdown_data: str,
        extensions: list[str],
        extension_configs: dict[str, dict[str, typing.Any]],
        site: siteconfig.SiteGenerator) -> str:
    # use markdown extensions in this util module if specified
    for i in range(len(extensions)):
        for ext_cls in (PageLinksExtension, HeaderAnchorExtension, InlineCodeClassExtension):
            if extensions[i] != ext_cls._name:
                continue
            kwargs = {}
            if hasattr(ext_cls, 'config') and isinstance(ext_cls.config, dict):
                if 'site' in ext_cls.config:
                    kwargs['site'] = site
                if config := extension_configs.get(ext_cls._name, None):
                    for k, v in config.items():
                        kwargs[k] = v
            extensions[i] = ext_cls(**kwargs)

    # fixup: add the default code style to codehilite.css_class
    if 'codehilite' in extension_configs:
        if 'css_class' in extension_configs['codehilite'] and isinstance(extension_configs['codehilite']['css_class'], str):
            if site.default_code_style not in extension_configs['codehilite']['css_class']:
                extension_configs['codehilite']['css_class'] += f' {site.default_code_style}'
        else:
            extension_configs['codehilite']['css_class'] = site.default_code_style

    return markdown.markdown(markdown_data,
        output_format='html5',
        extensions=extensions,
        extension_configs=extension_configs,
    )
