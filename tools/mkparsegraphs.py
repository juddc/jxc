import os
import enum
import typing
import dataclasses

import jxc
import _pyjxc

import railroad
from railroad import Diagram


class MatchType(enum.Enum):
    MatchGroupRef = enum.auto()
    Sequence = enum.auto()
    Choice = enum.auto()
    String = enum.auto()
    Character = enum.auto()
    CharacterRange = enum.auto()
    Whitespace = enum.auto()

    def _jxc_encode(self, doc: jxc.Serializer, enc: jxc.Encoder):
        doc.annotation("MatchType").expression_begin().identifier(self.name).expression_end()


class PatternType(enum.Enum):
    NoPattern = enum.auto()
    Optional = enum.auto()
    ZeroOrMore = enum.auto()
    OneOrMore = enum.auto()


@dataclasses.dataclass
class CharRange:
    start: int
    end: int
    exclusions: set[int]


@dataclasses.dataclass
class Match:
    match_type: MatchType
    value: str | list['Match'] | CharRange | typing.Optional['Match']
    pattern: PatternType = PatternType.NoPattern
    repeat: typing.Optional['Match'] = None

    @classmethod
    def _jxc_construct(cls, anno: list[jxc.Token]):
        assert len(anno) >= 4
        assert anno[0].type == jxc.TokenType.Identifier and anno[0].value == "Match"
        assert anno[1].type == jxc.TokenType.AngleBracketOpen

        flags: list[str] = []
        paren_value = None

        anno_idx = 2
        while anno_idx < len(anno):
            if paren_value is not None:
                match anno[anno_idx].type:
                    case jxc.TokenType.ParenClose:
                        if len(paren_value) > 0:
                            flags.append(paren_value)
                        paren_value = None
                    case jxc.TokenType.String:
                        paren_value.append(_pyjxc.parse_string_token(anno[anno_idx]))
                    case _:
                        raise ValueError(f"Unexpected token type {anno[anno_idx].type}")
            else:
                match anno[anno_idx].type:
                    case jxc.TokenType.Identifier:
                        flags.append(anno[anno_idx].value)
                    case jxc.TokenType.AngleBracketClose:
                        break
                    case jxc.TokenType.ParenOpen:
                        paren_value = []
                    case _:
                        raise ValueError(f"Unexpected token type {anno[anno_idx].type}")

            anno_idx += 1

        match_pattern = PatternType.NoPattern
        match_repeat = None
        if len(flags) > 1:
            match_pattern = getattr(PatternType, flags[0])
            flags = flags[1:]
            if match_pattern in (PatternType.ZeroOrMore, PatternType.OneOrMore) and isinstance(flags[0], list):
                assert len(flags[0]) == 1 and isinstance(flags[0][0], str)
                repeat_chars = flags[0][0]
                flags = flags[1:]
                if len(repeat_chars) == 1:
                    match_repeat = Match(MatchType.Character, repeat_chars)
                elif len(repeat_chars) > 1:
                    match_repeat = Match(MatchType.Choice, value=[ Match(MatchType.Character, ch) for ch in repeat_chars ])
        
        assert len(flags) == 1
        match_ty = getattr(MatchType, flags[0])

        def make(value) -> Match:
            return cls(match_type=match_ty, value=value, pattern=match_pattern, repeat=match_repeat)

        return make
        

    def _jxc_encode(self, doc: jxc.Serializer, enc: jxc.Encoder):
        type_args = [self.match_type.name]

        if self.pattern != PatternType.NoPattern:
            type_args.insert(0, self.pattern.name)

        doc.annotation(f'{self.__class__.__name__}<{(" ".join(type_args))}>')
        match self.match_type:
            case MatchType.MatchGroupRef:
                assert isinstance(self.value, str)
                doc.value_string(self.value)
            case MatchType.Sequence | MatchType.Choice:
                assert isinstance(self.value, list)
                doc.array_begin()
                for submatch in self.value:
                    submatch._jxc_encode(doc, enc)
                doc.array_end()
            case MatchType.String:
                assert isinstance(self.value, str)
                doc.value_string(self.value, jxc.StringQuoteMode.Double, decode_unicode=False)
            case MatchType.Character:
                assert isinstance(self.value, str) and len(self.value) == 1
                doc.value_string(self.value, jxc.StringQuoteMode.Single, decode_unicode=False)
            case MatchType.CharacterRange:
                assert isinstance(self.value, CharRange)
                enc.encode_value(self.value)
            case MatchType.Whitespace:
                doc.expression_empty()


@dataclasses.dataclass
class MatchGroup:
    name: str
    options: list['Match']


HTML_HEADER = """<!doctype html>
<html>
<head>
<style type="text/css">
html {
    background-color: #444;
    color: #eee;
}
.MatchGroupRef rect {
    fill:hsl(200, 100%, 90%) !important;
}
$CSS_BLOCK$
</style>
<body>
"""

HTML_FOOTER = """
</body>
</html>
"""


def codepoint_repr(codepoint: str) -> str:
    assert len(codepoint) == 1
    match codepoint:
        case ' ':
            return "' ' (space)"
        case '\t':
            return "'\\t' (tab)"
        case '\r':
            return "'\\r' (carriage return)"
        case '\n':
            return "line break"
        case _:
            return _pyjxc.debug_char_repr(codepoint, quote_char="'")


def match_to_node(match: Match):
    if match is None:
        return None

    result = None
    match match.match_type:
        case MatchType.MatchGroupRef:
            result = railroad.NonTerminal(match.value, cls=match.match_type.name)
        case MatchType.Sequence:
            assert isinstance(match.value, list)
            result = railroad.Sequence(*[ match_to_node(val) for val in match.value ])
        case MatchType.Choice:
            assert isinstance(match.value, list)
            result = railroad.Choice(0, *[ match_to_node(val) for val in match.value ])
        case MatchType.String:
            result = railroad.NonTerminal(_pyjxc.debug_string_repr(match.value), cls=match.match_type.name)
        case MatchType.Character:
            assert isinstance(match.value, str) and len(match.value) == 1
            result = railroad.NonTerminal(codepoint_repr(match.value), cls=match.match_type.name)
        case MatchType.CharacterRange:
            assert isinstance(match.value, CharRange)
            desc = f'{chr(match.value.start)} .. {chr(match.value.end)}'
            if len(match.value.exclusions) > 0:
                desc += ' (except ' + ', '.join(codepoint_repr(ch) for ch in sorted(list(match.value.exclusions))) + ')'
            result = railroad.NonTerminal(text=desc, cls=match.match_type.name)
        case MatchType.Whitespace:
            result = railroad.NonTerminal(text='whitespace', cls=match.match_type.name)
        case _:
            raise TypeError(f'Unhandled match_type {match.match_type!r}')

    match match.pattern:
        case PatternType.NoPattern:
            return result
        case PatternType.Optional:
            return railroad.Optional(result)
        case PatternType.ZeroOrMore:
            return railroad.ZeroOrMore(result, repeat=railroad.Group(match_to_node(match.repeat), label="separator") if match.repeat else None)
        case PatternType.OneOrMore:
            return railroad.OneOrMore(result, repeat=railroad.Group(match_to_node(match.repeat), label="separator") if match.repeat else None)


def match_group_to_node(group: MatchGroup):
    return railroad.Sequence(
        railroad.Start(label=group.name),
        railroad.Choice(0, *[ match_to_node(opt) for opt in group.options ]),
    )


def write_diagrams_to_html_file(html_output_path: str, diagrams: typing.Iterable[tuple[str, Diagram]]):
    with open(html_output_path, 'w') as fp:
        fp.write(HTML_HEADER.replace("$CSS_BLOCK$", railroad.DEFAULT_STYLE))
        for name, diag in diagrams:
            fp.write(f'<div class="node">\n')
            diag.writeSvg(fp.write)
            fp.write(f'</div>\n')
        fp.write(HTML_FOOTER)



if __name__ == "__main__":
    repo_root_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

    def lookup_annotation(ele: jxc.Element) -> jxc.decode.ValueConstructor:
        if len(ele.annotation) > 0 and ele.annotation[0].value == "Match":
            assert len(ele.annotation) >= 4
            return Match._jxc_construct(ele.annotation)
        return None

    with open(os.path.join(repo_root_dir, 'docs', 'jxc_syntax.jxc'), 'r') as fp:
        parser = jxc.Parser(fp.read())
        parser.set_find_construct_from_annotation_callback(lookup_annotation)
        group_dict: dict[str, Match] = parser.parse()
        groups: list[MatchGroup] = []
        for name, group in group_dict.items():
            groups.append(MatchGroup(name=name, options=group))

    diagrams: list[tuple[str, Diagram]] = []
    for group in groups:
        nodes = [railroad.Start(label=group.name)]
        if len(group.options) == 1:
            nodes.append(match_to_node(group.options[0]))
        elif len(group.options) > 1:
            nodes.append(railroad.Choice(0, *[ match_to_node(opt) for opt in group.options ]))

        diagrams.append((group.name, Diagram(*nodes, css=None)))

    write_diagrams_to_html_file(os.path.join(repo_root_dir, 'docs', 'jxc_syntax.html'), diagrams)

