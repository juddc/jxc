import os
import io
import typing
from dataclasses import dataclass
from enum import Enum, auto

import railroad
import jinja2

import jxc
import _pyjxc
from jxc import Token, TokenType, ElementType


class ParseError(ValueError):
    def __init__(self, msg: str, tok: Token, orig_buf: str):
        err = _pyjxc.ErrorInfo(msg, tok.start_idx, tok.end_idx)
        err.get_line_and_col_from_buffer(orig_buf)
        super().__init__(err.to_string(orig_buf))



def char_repr(ch: str, quote_char='`') -> str:
    assert isinstance(ch, str)
    assert len(ch) == 1
    match ch:
        case '\\':
            return f'{quote_char}\\{quote_char}'
        case ' ':
            return f'{quote_char} {quote_char} (space)'
        case '\t':
            return f'{quote_char}\\t{quote_char} (tab)'
        case '\n':
            return f'{quote_char}\\n{quote_char} (line break)'
        case '\r':
            return f'{quote_char}\\r{quote_char} (carriage return)'
        case _:
            return _pyjxc.debug_char_repr(ch, quote_char)



class MatchType(Enum):
    Start = auto()
    End = auto()
    MatchGroupRef = auto()
    Sequence = auto()
    Stack = auto()
    Choice = auto()
    HorizontalChoice = auto()
    String = auto()
    Character = auto()
    CharacterRange = auto()
    Group = auto()
    Comment = auto()
    Whitespace = auto()



class PatternType(Enum):
    OptionalCommon = auto()
    Optional = auto()
    ZeroOrMore = auto()
    OneOrMore = auto()



@dataclass
class CharRange:
    start: str
    end: str
    exclusions: typing.Optional[set[str]] = None

    def description(self) -> str:
        have_exc = self.exclusions is not None and len(self.exclusions) > 0

        exc_repr: list[str] = [char_repr(ch) for ch in sorted(list(self.exclusions))] if have_exc else []

        if not self.start and not self.end:
            result = 'Any UTF8 sequence'
            if have_exc:
                result += f' except {(", ".join(exc_repr))}'
            return result

        if not self.start and self.end:
            raise ValueError(f"Invalid character range - missing beginning of range")
        elif self.start and not self.end:
            raise ValueError(f"Invalid character range - missing end of range")

        assert self.start and self.end

        result = f'{char_repr(self.start)}..{char_repr(self.end)}'
        if have_exc:
            result += f' (except {(", ".join(exc_repr))})'
        return ''.join(result)

    @classmethod
    def parse(cls, value):
        if isinstance(value, (dict, list)) and len(value) == 0:
            return cls(start='', end='')

        if isinstance(value, list):
            match len(value):
                case 0:
                    return cls(start='', end='')
                case 2:
                    return cls(start=value[0], end=value[1])
                case 3:
                    return cls(start=value[0], end=value[1], exclusions=set(value[2]))
                case _:
                    raise ValueError(f'Expected list with 2 or 3 values, got {value!r}')

        elif isinstance(value, dict):
            valid_keys = ['start', 'end', 'exclusions']
            invalid_keys = set(value.keys()) - set(valid_keys)
            if len(invalid_keys) > 0:
                raise ValueError(f"Got one or more invalid keys {invalid_keys!r} (valid keys = {valid_keys!r})")
            return cls(start=value.get('start', ''), end=value.get('end', ''), exclusions=set(value.get('exclusions', [])))
        else:
            raise TypeError(f'Expected list or dict, got {value!r}')



class TokenParser:
    def __init__(self, tokens: _pyjxc.TokenSpan):
        self.tokens = tokens
        self.num_tokens = len(self.tokens)
        self.idx = 0

    def __len__(self):
        return self.num_tokens

    def __getitem__(self, token_index: int):
        return self.tokens[token_index]

    def advance(self):
        self.idx += 1
        return self.idx < self.num_tokens

    def last(self) -> bool:
        return self.idx == self.num_tokens - 1

    def done(self) -> bool:
        return self.idx >= self.num_tokens

    def current(self) -> Token:
        return self.tokens[self.idx] if self.idx < self.num_tokens else None

    def require_advance(self):
        self.idx += 1
        if self.idx >= self.num_tokens:
            raise ValueError(f'Unexpected end of stream')
    
    def require_done(self):
        if self.idx < self.num_tokens - 1:
            remainder = [ self.tokens[i] for i in range(self.idx, self.num_tokens) ]
            raise ValueError(f'Expected end of tokens, but still have unparsed remainder {remainder!r}')

    def require_token(self, ty: TokenType, val: typing.Optional[str] = None):
        if self.idx >= self.num_tokens:
            raise ValueError(f'Expected {ty}, got end of stream')
        if not self.tokens[self.idx].type == ty:
            raise ValueError(f'Expected {ty}, got {self.tokens[self.idx].type}')
        if val is not None and self.tokens[self.idx].value != val:
            raise ValueError(f'Expected {val}, got {self.tokens[self.idx].value}')

    def check_token(self, ty: TokenType, val: typing.Optional[str] = None) -> bool:
        if self.idx >= self.num_tokens:
            return False
        if not self.tokens[self.idx].type == ty:
            return False
        if val is not None and self.tokens[self.idx].value != val:
            return False
        return True
    
    def consume_token(self, ty: TokenType, val: typing.Optional[str] = None) -> typing.Optional[Token]:
        """
        If the current token matches, advances and returns the consumed Token.
        Otherwise returns None.
        """
        if self.check_token(ty, val):
            tok = self.current()
            self.advance()
            return tok
        return None



@dataclass
class PatternRepeat:
    values: list[str | int]
    group_name: typing.Optional[str] = None
    source: typing.Optional[tuple[int, int]] = None


    @staticmethod
    def _parse_count_number_token(tok: Token) -> int:
        number, number_suffix = _pyjxc.parse_number_token(tok)
        if isinstance(number, _pyjxc.ErrorInfo):
            raise ValueError(f'Failed parsing number: {number.to_string()}')
        elif not isinstance(number, int):
            raise ValueError(f'Expected integer, got {tok!r}')
        elif number_suffix != 'x':
            raise ValueError(f'Expected number with suffix "x", got {tok!r}')
        return number


    @classmethod
    def parse(cls, expr: _pyjxc.TokenSpan):
        if len(expr) == 0:
            raise ValueError("Expected repeat expression to contain strings")

        src_range = (expr[0].start_idx, expr[-1].end_idx)

        match expr:
            case [Token(TokenType.Number)]:
                return cls(
                    values=[PatternRepeat._parse_count_number_token(expr[0])],
                    source=src_range)
            case [Token(TokenType.Number), Token(TokenType.Comma), Token(TokenType.Number)]:
                return cls(values=[
                    PatternRepeat._parse_count_number_token(expr[0]),
                    PatternRepeat._parse_count_number_token(expr[2]),
                ], source=src_range)
            case [Token(TokenType.Number), Token(TokenType.Comma), Token(TokenType.String, grp)]:
                return cls(
                    values=[PatternRepeat._parse_count_number_token(expr[0])],
                    group_name=grp,
                    source=src_range)
            case [Token(TokenType.Number), Token(TokenType.Comma), Token(TokenType.Number), Token(TokenType.Comma), Token(TokenType.String, grp)]:
                return cls(
                    values=[
                        PatternRepeat._parse_count_number_token(expr[0]),
                        PatternRepeat._parse_count_number_token(expr[2]),
                    ],
                    group_name=grp,
                    source=src_range)

        values = []
        group_name = None
        parser = TokenParser(expr)
        while True:
            parser.require_token(TokenType.String)
            values.append(_pyjxc.parse_string_token(parser.current()))
            if not parser.advance():
                break

            if parser.consume_token(TokenType.ExpressionOperator, '|'):
                # pipe -> back to top for another value
                continue
            elif parser.check_token(TokenType.Comma):
                # comma (trailing) -> done
                if parser.last():
                    break
                # comma -> group_name -> done
                parser.require_advance()
                parser.require_token(TokenType.String)
                group_name = _pyjxc.parse_string_token(parser.current())
                parser.advance()
                break
            else:
                raise ValueError(f"Unexpected token {parser.current()!r}")

        # allow trailing comma
        parser.consume_token(TokenType.Comma)
        parser.require_done()

        return cls(values=values, group_name=group_name, source=src_range)



@dataclass
class Match:
    match_type: MatchType
    value: typing.Union[str, list['Match'], CharRange, 'Match']
    pattern: typing.Optional[PatternType] = None
    repeat: typing.Optional['Match'] = None
    group: typing.Optional[str] = None
    source: typing.Optional[tuple[int, int]] = None


    @classmethod
    def from_value(cls, val: str | int, group_name: typing.Optional[str] = None):
        if isinstance(val, int):
            match_type = MatchType.Comment
            val = f'exactly {val}x'
        else:
            assert isinstance(val, str)
            match len(val):
                case 0:
                    match_type = MatchType.Whitespace
                case 1:
                    match_type = MatchType.Character
                case _:
                    match_type = MatchType.String
        return cls(match_type, value=val, group=group_name)


    @classmethod
    def from_pattern_repeat(cls, repeat: PatternRepeat):
        assert len(repeat.values) > 0
        if len(repeat.values) == 1:
            result = cls.from_value(repeat.values[0])
            result.source = repeat.source
            return result
        elif len(repeat.values) == 2 and all(isinstance(v, int) for v in repeat.values):
            return cls(MatchType.Comment, value=f'{repeat.values[0]}x to {repeat.values[1]}x', group=repeat.group_name, source=repeat.source)
        else:
            return cls(MatchType.Choice, value=[ Match.from_value(val) for val in repeat.values ], group=repeat.group_name, source=repeat.source)


    @classmethod
    def make_parse_func(cls, ty: MatchType, pat: typing.Optional[PatternType] = None):
        def parse_value(value):
            repeat_meta: typing.Optional[PatternRepeat] = None
            group: typing.Optional[str] = None
            if isinstance(value, (str, CharRange)):
                match_value = value
            elif ty == MatchType.Group:
                match value:
                    case { 'name': group_name, 'value': group_inner }:
                        assert isinstance(group_name, str)
                        group = group_name
                        match_value = group_inner
                    case _:
                        raise ValueError(f'Invalid group (expected object with "name" and "value" keys) {value!r}')
            elif pat is not None:
                # if a pattern is specified, check if the value contains a repeat directive
                if isinstance(value, list) and len(value) > 0 and isinstance(value[0], PatternRepeat):
                    if len(value) != 2:
                        raise ValueError(f"Expected list in the form [repeat, value], got {value!r}")
                    repeat_meta, match_value = value
                else:
                    match_value = value
            elif isinstance(value, list):
                if not all(isinstance(item, Match) for item in value):
                    raise TypeError(f'Expected a list containing all matches, got {value!r}')
                match_value = value
            else:
                raise TypeError(f"Invalid match value {value!r}")

            repeat = None
            if repeat_meta is not None:
                assert isinstance(repeat_meta, PatternRepeat)
                repeat = cls.from_pattern_repeat(repeat_meta)

            return cls(match_type=ty, value=match_value, pattern=pat, repeat=repeat, group=group)

        return parse_value


    def to_node(self) -> 'railroad.Node':
        result = None

        match self.match_type:
            case MatchType.Start:
                result = railroad.Start(label=self.value)
            case MatchType.End:
                result = railroad.End()
            case MatchType.MatchGroupRef:
                result = railroad.NonTerminal(self.value, cls=self.match_type.name)
            case MatchType.Sequence | MatchType.Stack:
                if not isinstance(self.value, list):
                    raise ValueError(f"Expected list of values, got {self.value!r}")
                result = getattr(railroad, self.match_type.name)(*[ val.to_node() for val in self.value ])
            case MatchType.Choice:
                if not isinstance(self.value, list):
                    raise ValueError(f"Expected list of values, got {self.value!r}")
                result = railroad.Choice(0, *[ val.to_node() for val in self.value ])
            case MatchType.HorizontalChoice:
                if not isinstance(self.value, list):
                    raise ValueError(f"Expected list of values, got {self.value!r}")
                result = railroad.HorizontalChoice(*[ val.to_node() for val in self.value ])
            case MatchType.String:
                result = railroad.NonTerminal(_pyjxc.debug_string_repr(self.value, quote_char='`'), cls=self.match_type.name)
            case MatchType.Character:
                if not isinstance(self.value, str) or len(self.value) != 1:
                    raise ValueError(f"Expected single character, got {self.value!r}")
                result = railroad.NonTerminal(char_repr(self.value), cls=self.match_type.name)
            case MatchType.CharacterRange:
                if not isinstance(self.value, CharRange):
                    raise ValueError(f"Expected CharRange, got {self.value!r}")
                result = railroad.NonTerminal(text=self.value.description(), cls=self.match_type.name)
            case MatchType.Group:
                result = railroad.Group(item=self.value.to_node(), label=self.group)
            case MatchType.Comment:
                result = railroad.Comment(text=self.value)
            case MatchType.Whitespace:
                result = railroad.NonTerminal(text='whitespace', cls=self.match_type.name)
            case _:
                raise TypeError(f'Unhandled match_type {self.match_type!r}')

        match self.pattern:
            case None:
                pass
            case PatternType.OptionalCommon:
                result = railroad.Optional(result, skip=False)
            case PatternType.Optional:
                result = railroad.Optional(result, skip=True)
            case PatternType.ZeroOrMore:
                result = railroad.ZeroOrMore(result, repeat=self.repeat.to_node() if self.repeat else None)
            case PatternType.OneOrMore:
                result = railroad.OneOrMore(result, repeat=self.repeat.to_node() if self.repeat else None)

        if self.match_type != MatchType.Group and self.group:
            result = railroad.Group(result, label=self.group)

        return result



class SyntaxParser:
    def __init__(self, buf: str):
        self.buf = buf
        self.parser = jxc.Parser(buf)
        self.parser.set_find_construct_from_annotation_callback(self.parse_annotation)
        self._refs = set()

    def parse_annotation(self, ele: jxc.Element) -> jxc.decode.ValueConstructor:
        #TODO: investigate why converting to list is needed here
        match list(ele.annotation):
            case []:
                return None
            case [Token(TokenType.Identifier, ident)]:
                return self._find_construct(ident)
            case [Token(TokenType.Identifier, ident), Token(TokenType.AngleBracketOpen), Token(TokenType.Identifier, ident_inner), Token(TokenType.AngleBracketClose)]:
                return self._find_construct(ident, ident_inner)
            case _:
                raise ParseError(f'Invalid annotation {ele.annotation.source()!r}', ele.token, self.buf)

    def _find_construct(self, ident: str, ident_inner: typing.Optional[str] = None) -> jxc.decode.ValueConstructor:
        pat = getattr(PatternType, ident_inner) if ident_inner is not None else None
        if ident == 'CharacterRange':
            def parse_range(value):
                return Match(match_type=MatchType.CharacterRange, value=CharRange.parse(value), pattern=pat)
            return parse_range
        elif ident == 'repeat':
            return (PatternRepeat.parse, jxc.ExpressionParseMode.TokenList)
        elif hasattr(MatchType, ident):
            return Match.make_parse_func(getattr(MatchType, ident), pat)
        return None

    def parse(self):
        return self.parser.parse()



def write_diagrams_to_html_file(html_output_path: str, diagrams: typing.Iterable[tuple[str, railroad.Diagram]]):
    env = jinja2.Environment(loader=jinja2.FileSystemLoader('./docs/templates'))
    tmpl = env.loader.load(env, 'syntax_diagrams.html')

    diagram_source = {}
    for name, diag in diagrams:
        diagram_writer = io.StringIO()
        diag.writeSvg(diagram_writer.write)
        diagram_source[name] = diagram_writer.getvalue()
    
    with open(html_output_path, 'w') as fp:
        fp.write(tmpl.render({
            'page_title': "Syntax Specification",
            'railroad_css': railroad.DEFAULT_STYLE,
            'diagrams': diagram_source,
        }))



def match_tree_iter(root: Match, depth=0) -> typing.Iterable[tuple[Match, int]]:
    """
    Generator that recursively goes through all nodes in a Match tree
    """
    yield (root, depth)
    if isinstance(root.value, Match):
        yield from match_tree_iter(root.value, depth=depth + 1)
    elif isinstance(root.value, list):
        for val in root.value:
            if isinstance(val, Match):
                yield from match_tree_iter(val, depth=depth + 1)



def validate_match_group_ref_names(all_groups: dict[str, Match]):
    all_refs: set[str] = set()
    for val in all_groups.values():
        for match, _depth in match_tree_iter(val):
            #print('    ' * _depth, match.match_type, str(match.value)[:50])
            if match.match_type == MatchType.MatchGroupRef:
                assert isinstance(match.value, str)
                all_refs.add(match.value)

    for ref_name in all_refs:
        if ref_name not in all_groups:
            raise ValueError(f'MatchGroupRef {ref_name} is not defined')


if __name__ == "__main__":
    repo_root_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    generated_docs_output_dir = os.path.join(repo_root_dir, 'build', 'docs')

    with open(os.path.join(repo_root_dir, 'docs', 'jxc_syntax.jxc'), 'r') as fp:
        parser = SyntaxParser(fp.read())
        groups: dict[str, Match] = parser.parse()
    
    validate_match_group_ref_names(groups)

    diagrams: list[tuple[str, railroad.Diagram]] = []
    for group_name, match in groups.items():
        try:
            start_node = railroad.Start(label=group_name, type='complex')
            start_node.attrs['class'] = 'start'
            diagrams.append((group_name, railroad.Diagram(start_node, match.to_node(), css=None, type='complex')))
        except Exception as e:
            if match.source:
                err = _pyjxc.ErrorInfo(f"{e.__class__.__name__}: {e}", match.source[0], match.source[1])
                err.get_line_and_col_from_buffer(parser.buf)
                raise Exception(err.to_string(parser.buf))
            else:
                print(f"Failed converting {group_name} to diagram")
                raise

    os.makedirs(generated_docs_output_dir, exist_ok=True)

    write_diagrams_to_html_file(os.path.join(generated_docs_output_dir, 'jxc_syntax_diagrams.html'), diagrams)

