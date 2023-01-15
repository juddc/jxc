import typing
from enum import Enum
from dataclasses import dataclass
import jxc


@dataclass
class Template:
    file: str
    vars: typing.Optional[dict[str, typing.Any]] = None

    def make_context(self, **kwargs) -> dict[str, typing.Any]:
        ctx = { key: val for key, val in kwargs.items() }
        if self.vars is not None:
            for key, val in self.vars.items():
                ctx[key] = val
        return ctx


@dataclass
class BasePage:
    title: str
    template: Template
    output: str


@dataclass
class MarkdownPage(BasePage):
    source: str


@dataclass
class RailroadDiagramsPage(BasePage):
    source: str


@dataclass
class JXCPage(BasePage):
    source: str


class CodeStyleType(Enum):
    Dark = 'Dark'
    Light = 'Light'


@dataclass
class DocGen:
    code_styles: dict[str, CodeStyleType]
    default_code_style: str
    global_template_vars: typing.Optional[dict[str, typing.Any]]
    pages: list[BasePage]


def parse(docgen_path: str) -> DocGen:
    annotation_classes = [
        (DocGen, jxc.ClassConstructMode.DictAsKeywordArgs),
        (CodeStyleType, jxc.ClassConstructMode.ListAsArgs),
        (Template, jxc.ClassConstructMode.DictAsKeywordArgs),
        (MarkdownPage, jxc.ClassConstructMode.DictAsKeywordArgs),
        (RailroadDiagramsPage, jxc.ClassConstructMode.DictAsKeywordArgs),
        (JXCPage, jxc.ClassConstructMode.DictAsKeywordArgs),
    ]

    with open(docgen_path, 'r') as fp:
        result = jxc.loads(
            fp.read(),
            decode_enum=True,
            ignore_unknown_annotations=False,
            annotation_hooks=[(cls.__name__, (cls, mode)) for (cls, mode) in annotation_classes])

    if not isinstance(result, DocGen):
        raise ValueError(f"Failed to parse docgen: Expected DocGen type, got {type(result)}")

    return result
