import typing
from enum import Enum
from dataclasses import dataclass
from . import code_style
import jxc


@dataclass
class Template:
    file: str
    vars: typing.Optional[dict[str, typing.Any]] = None


@dataclass
class BasePage:
    title: str
    template: Template
    output: str

    def get_source_filename(self) -> typing.Optional[str]:
        return None


@dataclass
class MarkdownPage(BasePage):
    source: str
    markdown_extensions: typing.Optional[dict[str, dict[str, typing.Any]]] = None

    def get_source_filename(self) -> typing.Optional[str]:
        return self.source


@dataclass
class HTMLPage(BasePage):
    source: str

    def get_source_filename(self) -> typing.Optional[str]:
        return self.source


@dataclass
class RailroadDiagramsPage(BasePage):
    source: str

    def get_source_filename(self) -> typing.Optional[str]:
        return self.source


@dataclass
class JXCPage(BasePage):
    source: str
    render_header_comments_as_markdown: bool = True
    markdown_extensions: typing.Optional[dict[str, dict[str, typing.Any]]] = None

    def get_source_filename(self) -> typing.Optional[str]:
        return self.source


@dataclass
class SiteGenerator:
    pages: list[BasePage]
    enable_search: bool = True
    extra_static_files: typing.Optional[list[str]] = None
    code_styles: typing.Optional[list[str]] = None
    default_code_style: typing.Optional[str] = None
    code_css_class: typing.Optional[str] = None
    template_dir: typing.Optional[str] = None
    static_dir: typing.Optional[str] = None
    markdown_extensions: typing.Optional[dict[str, dict[str, typing.Any]]] = None
    global_template_vars: typing.Optional[dict[str, typing.Any]] = None


def make_template_context(site: SiteGenerator, page: BasePage, **kwargs) -> dict[str, typing.Any]:
    ctx = {}
    if site.global_template_vars:
        ctx = { **ctx, **site.global_template_vars }
    if page.template and page.template.vars:
        ctx = { **ctx, **page.template.vars }
    return { **ctx, **kwargs }


def make_markdown_context(site: SiteGenerator, page: BasePage) -> tuple[list[str], dict[str, dict[str, typing.Any]]]:
    ext_list = []
    ext_cfg = {}

    def push_exts(exts: dict[str, dict[str, typing.Any]]):
        if not exts:
            return
        for name, cfg in exts.items():
            if name not in ext_list:
                ext_list.append(name)
            if cfg:
                if name not in ext_cfg:
                    ext_cfg[name] = cfg
                else:
                    ext_cfg[name] = { **ext_cfg[name], **cfg }

    push_exts(site.markdown_extensions)
    if isinstance(page, MarkdownPage) or (isinstance(page, JXCPage) and page.render_header_comments_as_markdown):
        push_exts(page.markdown_extensions)

    return ext_list, ext_cfg


def parse(docgen_path: str) -> SiteGenerator:
    annotation_classes = []

    for dataclass_cls in (SiteGenerator, MarkdownPage, HTMLPage, RailroadDiagramsPage, JXCPage, Template):
        annotation_classes.append((dataclass_cls, jxc.ClassConstructMode.DictAsKeywordArgs))

    with open(docgen_path, 'r') as fp:
        result = jxc.loads(
            fp.read(),
            ignore_unknown_annotations=False,
            ignore_unknown_number_suffixes=False,
            annotation_hooks=[(cls.__name__, (cls, mode)) for (cls, mode) in annotation_classes])

    if not isinstance(result, SiteGenerator):
        raise ValueError(f"Failed to parse docgen: Expected SiteGenerator type, got {type(result)}")

    return result
