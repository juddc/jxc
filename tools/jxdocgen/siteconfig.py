import os
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
class GeneratedStaticPage(BasePage):
    pass


_STATIC_FILE_INSTANCES = set()
_STATIC_FILE_DATA = {}


@dataclass
class StaticFile:
    path: str

    def __post_init__(self):
        _STATIC_FILE_INSTANCES.add(self.path)

    def __str__(self):
        if self.path in _STATIC_FILE_DATA:
            return _STATIC_FILE_DATA[self.path]
        else:
            return f'(STATIC FILE {self.path})'


_DEFAULT_STATIC_FILE_EXTENSIONS = { 'css', 'js', 'jpg', 'gif', 'png', 'svg' }


@dataclass
class SiteGenerator:
    pages: list[BasePage]
    extra_static_files: typing.Optional[list[str]] = None
    generated_static: typing.Optional[list[BasePage]] = None
    code_styles: typing.Optional[list[str]] = None
    default_code_style: typing.Optional[str] = None
    code_css_class: typing.Optional[str] = None
    template_dir: typing.Optional[str] = None
    static_dir: typing.Optional[str] = None
    markdown_extensions: typing.Optional[dict[str, dict[str, typing.Any]]] = None
    global_template_vars: typing.Optional[dict[str, typing.Any]] = None
    static_file_extensions: typing.Optional[list[str]] = None


    def all_static_files(self, static_dir: typing.Optional[str]) -> typing.Iterable[str]:
        static_exts = set(self.static_file_extensions) if self.static_file_extensions is not None else _DEFAULT_STATIC_FILE_EXTENSIONS
        if static_dir is not None:
            for name in os.listdir(static_dir):
                if any(name.endswith(f'.{ext}') for ext in static_exts):
                    yield os.path.abspath(os.path.join(static_dir, name))

        # then, pull any additional static files that were specified explicitly
        if self.extra_static_files:
            for file_path in self.extra_static_files:
                file_path = os.path.abspath(file_path)
                if os.path.exists(file_path):
                    yield file_path
                else:
                    raise FileNotFoundError(file_path)


    def resolve_static_file_instances(self, static_dir: typing.Optional[str], dev_server: bool = False):
        if len(_STATIC_FILE_INSTANCES) == 0:
            return
        all_static_file_paths = list(self.all_static_files(static_dir))
        for inst_path in _STATIC_FILE_INSTANCES:
            assert isinstance(inst_path, str)
            if not dev_server and inst_path in _STATIC_FILE_DATA:
                continue
            # find the actual full path that the StaticFile path is referring to
            for path in all_static_file_paths:
                if os.path.normpath(path).replace('\\', '/').endswith(inst_path.replace('\\', '/')):
                    with open(path, 'r') as fp:
                        _STATIC_FILE_DATA[inst_path] = fp.read()
                    break


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

    for cls in (SiteGenerator, MarkdownPage, HTMLPage, RailroadDiagramsPage, JXCPage, GeneratedStaticPage, Template):
        annotation_classes.append((cls, jxc.ClassConstructMode.DictAsKeywordArgs))

    for cls in (StaticFile, ):
        annotation_classes.append((cls, jxc.ClassConstructMode.ListAsArgs))

    with open(docgen_path, 'r') as fp:
        result = jxc.loads(
            fp.read(),
            ignore_unknown_annotations=False,
            ignore_unknown_number_suffixes=False,
            annotation_hooks={ cls.__name__: (cls, mode) for (cls, mode) in annotation_classes })

    if not isinstance(result, SiteGenerator):
        raise ValueError(f"Failed to parse docgen: Expected SiteGenerator type, got {type(result)}")

    return result
