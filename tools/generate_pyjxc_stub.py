#!/usr/bin/env python3
import os
import sys
import re
import typing
import inspect
import textwrap
import argparse
from dataclasses import dataclass

_parser = argparse.ArgumentParser()
_parser.add_argument("--module-path", '-m', type=str, help='Path to add to sys.path to import _pyjxc')
_parser.add_argument("--output", '-o', type=str, required=True, help='Stub file output path')
_script_args = _parser.parse_args()

if _script_args.module_path:
    sys.path.insert(0, _script_args.module_path)
elif os.path.exists('./build/debug'):
    sys.path.insert(0, './build/debug')

try:
    import _pyjxc
except ImportError as e:
    print(f"Error: Failed to import _pyjxc: {e}")
    sys.exit(1)

assert _script_args.output.endswith(".pyi")

DEBUG_DOCSTRINGS = False

FUNC_SIG_REGEX = re.compile(r'([\w.]+)?\((.*)\)(?:\s*->\s*([\w\:\<\>\,\.\'\"]+))?')
FUNC_ARG_REGEX = re.compile(r'(\w+)(\s*:\s*[^=:]+)?(\s*=\s*[^=:]+)?')
ENUM_VALUE_REGEX = re.compile(r'<([\w\.]+): ([0-9]+)>')
FUNC_OVERLOAD_DOCSTR_REGEX = re.compile(r'([0-9]+)\.(?:\s*)([a-zA-Z0-9_]+)')

INVALID_IDX_CONSTANT = _pyjxc.invalid_idx

# invalid_idx should be exactly 64 `1`s in binary form
assert isinstance(INVALID_IDX_CONSTANT, int)
assert len(bin(INVALID_IDX_CONSTANT)[2:]) == 64 and all(ch == '1' for ch in bin(INVALID_IDX_CONSTANT)[2:])

MODULE_NAME = "_pyjxc"


ATTRIBUTES_BLACKLIST = set([
    f"__{attr}__" for attr in (
        "file",
        "loader",
        "name",
        "package",
        "spec",
        "path",
        "cached",
        "builtins",
        "class",
        "delattr",
        "dir",
        "format",
        "getattribute",
        "init_subclass",
        "module",
        "new",
        "reduce",
        "reduce_ex",
        "sizeof",
        "subclasshook",
    )
])


PYTHON_KEYWORDS = {
    # python keywords
    "False",
    "await",
    "else",
    "import",
    "pass",
    "None",
    "break",
    "except",
    "in",
    "raise",
    "True",
    "class",
    "finally",
    "is",
    "return",
    "and",
    "continue",
    "for",
    "lambda",
    "try",
    "as",
    "def",
    "from",
    "nonlocal",
    "while",
    "assert",
    "del",
    "global",
    "not",
    "with",
    "async",
    "elif",
    "if",
    "or",
    "yield",
}


PYTHON_BUILTINS = {
    # python builtins - not really keywords, but we should treat them as such
    "abs",
    "delattr",
    "hash",
    "memoryview",
    "set",
    "all",
    "dict",
    "help",
    "min",
    "setattr",
    "any",
    "dir",
    "hex",
    "next",
    "slice",
    "ascii",
    "divmod",
    "id",
    "object",
    "sorted",
    "bin",
    "enumerate",
    "input",
    "oct",
    "staticmethod",
    "bool",
    "eval",
    "int",
    "open",
    "str",
    "breakpoint",
    "exec",
    "isinstance",
    "ord",
    "sum",
    "bytearray",
    "filter",
    "issubclass",
    "pow",
    "super",
    "bytes",
    "float",
    "iter",
    "print",
    "tuple",
    "callable",
    "format",
    "len",
    "property",
    "type",
    "chr",
    "frozenset",
    "list",
    "range",
    "vars",
    "classmethod",
    "getattr",
    "locals",
    "repr",
    "zip",
    "compile",
    "globals",
    "map",
    "reversed",
    "complex",
    "hasattr",
    "max",
    "round",
}


BOOL_FUNC_NAMES = set([f'__{op}__' for op in ("bool", "eq", "ne", "lt", "le", "gt", "ge")])
INT_FUNC_NAMES = set([f'__{op}__' for op in ("len", )])
STR_FUNC_NAMES = set([f'__{op}__' for op in ("str", "repr")])


def is_module(member: typing.Any) -> bool:
    return inspect.ismodule(member)


def is_function(member: typing.Any) -> bool:
    return callable(member) and (
        inspect.isbuiltin(member)
        or inspect.isroutine(member)
        or inspect.isfunction(member)
        or inspect.ismethod(member)
        or type(member).__name__ == "nb_func")


def is_static_method(member: typing.Any, parent: typing.Any = None) -> bool:
    return (
        (inspect.isroutine(member) or inspect.isfunction(member))
        and not member.__name__ == "__init__"
        and not inspect.ismethod(member)
        and not type(member).__name__ == "instancemethod"
        and parent is None
    )


def is_class(member: typing.Any) -> bool:
    return isinstance(member, type) and inspect.isclass(member)


def is_enum(member: typing.Any) -> bool:
    return isinstance(member, type) and hasattr(member, '__entries')


def is_attr(member: typing.Any) -> bool:
    if hasattr(member, "__name__") and member.__name__ not in ATTRIBUTES_BLACKLIST:
        return False
    return (not is_module(member)
        and not is_function(member)
        and not is_class(member)
        and not is_enum(member)
        and not isinstance(member, type))


def tab(indent: int) -> str:
    return "    " * indent


class InterfaceFile:
    def __init__(self, filename):
        self.fp = open(filename, 'w')
        self.level = 0

    def close(self):
        self.fp.close()
    
    def indent(self, delta: int = 1):
        self.level += delta
    
    def dedent(self, delta: int = 1):
        self.level = max(0, self.level - delta)

    def write(self, text: str, indent: bool = False):
        if indent:
            self.fp.write(tab(self.level))
        self.fp.write(text)

    def writeln(self, line: str = "", indent: int = -1):
        if line:
            if indent < 0:
                indent = self.level
            self.fp.write(f'{tab(indent)}{line}\n')
        else:
            self.fp.write("\n")

    def write_docstring(self, docstr: str):
        if not isinstance(docstr, str) or len(docstr) == 0:
            return
        self.writeln('"""')
        if DEBUG_DOCSTRINGS:
            self.writeln(docstr)
        else:
            for line in docstr.split("\n"):
                for subline in textwrap.wrap(line.rstrip(), 80):
                    self.writeln(subline)
        self.writeln('"""')


def parse_cpp_type(cpptype: str) -> str:
    cpptype = cpptype.replace("::", ".")
    cpptype = cpptype.replace(":", ".")
    for int_type in ("unsigned long", "long", "int", "short"):
        cpptype = cpptype.replace(int_type, "int")
    for float_type in ("float", "double", "long double"):
        cpptype = cpptype.replace(float_type, "float")
    cpptype = cpptype.replace('std.optional', 'typing.Optional')
    cpptype = cpptype.replace("<", "[")
    cpptype = cpptype.replace(">", "]")
    return cpptype


@dataclass
class FunctionArg:
    name: str
    arg_type: typing.Optional[str] = None
    default_val: typing.Optional[str] = None

    def to_string(self) -> str:
        parts = [self.name]
        if self.arg_type:
            parts.append(f': {self.arg_type}')
        if self.default_val:
            parts.append(f' = {self.default_val}')
        return ''.join(parts)


@dataclass
class FunctionSig:
    name: str
    args: typing.Optional[list[typing.Optional[FunctionArg]]] = None
    ret_type: typing.Optional[str] = None
    is_overload: bool = False

    def to_string(self, is_method=False):
        func_args = []
        if self.args:
            for arg_idx, arg in enumerate(self.args):
                arg_name = arg.name if arg and arg.name else None
                if arg_name is None:
                    arg_name = "self" if (is_method and arg_idx == 0) else f"_unnamed_arg{arg_idx}"
                arg_parts = [arg_name]
                if arg:
                    if arg.arg_type:
                        arg_parts.append(f': {arg.arg_type}')
                    if arg.default_val:
                        arg_parts.append(f' = {arg.default_val}')
                func_args.append("".join(arg_parts))
        arg_str = ", ".join(func_args)
        suffix = f' -> {self.ret_type}' if self.ret_type else ''
        return f'{self.name}({arg_str}){suffix}'


def _lex(value: str):
    lexer = _pyjxc.Lexer(value)
    tok: _pyjxc.Token = lexer.next()
    while tok.type != _pyjxc.TokenType.EndOfStream:
        if tok.type == _pyjxc.TokenType.Invalid:
            raise ValueError(lexer.get_error().to_repr(value))
        yield tok
        tok = lexer.next()


def _parse_args(value: str) -> list[FunctionArg]:
    args: list[list[_pyjxc.Token]] = []

    paren_depth = 0
    bracket_depth = 0
    brace_depth = 0
    arg_parts: list[_pyjxc.Token] = []
    for tok in _lex(value):
        assert isinstance(tok, _pyjxc.Token)
        match tok.type:
            case _pyjxc.TokenType.ParenOpen:
                paren_depth += 1
            case _pyjxc.TokenType.ParenClose:
                paren_depth -= 1
            case _pyjxc.TokenType.SquareBracketOpen:
                bracket_depth += 1
            case _pyjxc.TokenType.SquareBracketClose:
                bracket_depth -= 1
            case _pyjxc.TokenType.BraceOpen:
                brace_depth += 1
            case _pyjxc.TokenType.BraceClose:
                brace_depth -= 1
            case _pyjxc.TokenType.Comma if paren_depth == 0 and bracket_depth == 0 and brace_depth == 0:
                args.append(arg_parts)
                arg_parts = []
                continue
            case _:
                pass

        arg_parts.append(tok.copy())

    if len(arg_parts) > 0:
        args.append(arg_parts)

    def merge_token_list(orig_value: str, token_list: list[_pyjxc.Token]) -> str:
        match len(token_list):
            case 0:
                return ''
            case 1:
                if token_list[0].type in (_pyjxc.TokenType.True_, _pyjxc.TokenType.False_):
                    return token_list[0].value.capitalize()
                return token_list[0].value
            case _:
                # capitalize True and False literals
                orig_value_chars = [ ch for ch in orig_value ]
                for i, tok in enumerate(token_list):
                    if tok.type == _pyjxc.TokenType.True_:
                        orig_value_chars[tok.start_idx] = 'T'
                    elif tok.type == _pyjxc.TokenType.False_:
                        orig_value_chars[tok.start_idx] = 'F'
                return ''.join(orig_value_chars[token_list[0].start_idx : token_list[-1].end_idx])

    # merge result tokens
    result: list[FunctionArg] = []
    for arg_tokens in args:
        idx = 0

        name_parts: list[_pyjxc.Token] = []
        while idx < len(arg_tokens):
            match arg_tokens[idx].type:
                case _pyjxc.TokenType.Asterisk | _pyjxc.TokenType.Identifier:
                    name_parts.append(arg_tokens[idx])
                    idx += 1
                case _pyjxc.TokenType.Colon:
                    idx += 1
                    break
        
        type_parts: list[_pyjxc.Token] = []
        bracket_depth = 0
        while idx < len(arg_tokens):
            match arg_tokens[idx].type:
                case _pyjxc.TokenType.Identifier | _pyjxc.TokenType.Period:
                    type_parts.append(arg_tokens[idx])
                    idx += 1
                case _pyjxc.TokenType.SquareBracketOpen:
                    type_parts.append(arg_tokens[idx])
                    bracket_depth += 1
                    idx += 1
                case _pyjxc.TokenType.SquareBracketClose:
                    type_parts.append(arg_tokens[idx])
                    bracket_depth -= 1
                    idx += 1
                case _pyjxc.TokenType.Equals if bracket_depth == 0:
                    idx += 1
                    break
        
        assert bracket_depth == 0
        default_value_parts: list[_pyjxc.Token] = []
        while idx < len(arg_tokens):
            default_value_parts.append(arg_tokens[idx])
            idx += 1
    
        func_arg = FunctionArg(
            name=merge_token_list(value, name_parts),
            arg_type=merge_token_list(value, type_parts),
            default_val=merge_token_list(value, default_value_parts))

        if func_arg.default_val == str(INVALID_IDX_CONSTANT):
            func_arg.default_val = "invalid_idx"
        
        if func_arg.arg_type == 'sequence':
            func_arg.arg_type = 'typing.Sequence'

        result.append(func_arg)

    return result



def parse_function_sig(sig: str) -> typing.Optional[FunctionSig]:
    # example sig:
    # set(self: _pyjxc.ErrorInfo, new_message: str, new_buffer_start_idx: int = 18446744073709551615,
    #     new_buffer_end_idx: int = 18446744073709551615) -> None

    # replace the weird enum literal syntax `<EnumType.Value : 0>` with just the type and name parts
    sig = re.sub(ENUM_VALUE_REGEX, r'\1', sig)

    #print(f'parse_function_sig({sig!r})')
    if pat := FUNC_SIG_REGEX.match(sig):
        sig_name, sig_args, sig_rettype = pat.groups()
    else:
        return None

    #print(f'\t{sig_name=!r} {sig_args=!r} {sig_rettype=!r}')

    if sig_rettype:
        sig_rettype = sig_rettype.replace("::", ".")
        if sig_rettype.startswith("jxc."):
            sig_rettype = sig_rettype[4:]

    return FunctionSig(name=sig_name, args=_parse_args(sig_args), ret_type=sig_rettype)



def strip_overloads_from_docstring(doc: str) -> str:
    """
    Strips the "Overloaded function" and following function signatures data from a docstring.
    """
    result = []
    overload_block = False
    for line in doc.split('\n'):
        stripped_line = line.strip()
        if not overload_block and stripped_line == 'Overloaded function.':
            overload_block = True
            continue

        if overload_block:
            if sig := FUNC_OVERLOAD_DOCSTR_REGEX.match(stripped_line):
                continue
            elif len(stripped_line) > 0:
                overload_block = False

        if not overload_block:
            result.append(line)

    return '\n'.join(result)



def parse_docstring(item: typing.Any, item_name: typing.Optional[str] = None, orig_docstring: bool = False) -> tuple[list[FunctionSig], str]:
    if not hasattr(item, "__doc__") or not isinstance(item.__doc__, str) or len(item.__doc__) == 0:
        return [], ''

    if item_name is None:
        item_name = getattr(item, '__name__', '')
    assert isinstance(item_name, str)

    docstr = item.__doc__ + ''

    sigs: list[FunctionSig] = []
    is_overloaded = False

    if is_function(item):
        docstr_lines = []
        for line in docstr.split("\n"):
            line_clean = line.strip()
            if len(line_clean) == 0:
                docstr_lines.append(line_clean)
            elif "Overloaded function." in line_clean:
                is_overloaded = True
                print(item, item_name)
                docstr_lines.append(line)
            elif func_sig := parse_function_sig(line_clean):
                sigs.append(func_sig)
            else:
                docstr_lines.append(line)

        if is_overloaded:
            overloads = []
            in_overloads = False
            for line in docstr.split("\n"):
                line = line.strip()
                if "Overloaded function." in line:
                    in_overloads = True
                elif in_overloads and len(line) > 0 and line[0].isnumeric():
                    if func_matches := FUNC_OVERLOAD_DOCSTR_REGEX.match(line):
                        assert func_matches.groups()[1] == item_name
                        expected_prefix = f'{func_matches.groups()[0]}. '
                        assert line.startswith(expected_prefix)
                        line = line[len(expected_prefix):]
                        overloads.append(line)

            # replace single function sig with all overloads
            if len(sigs) == 1 and len(overloads) > 0:
                sigs = []
            for overload in overloads:
                if overload_sig := parse_function_sig(overload):
                    overload_sig.is_overload = True
                    sigs.append(overload_sig)

        # strip empty lines from the front
        while len(docstr_lines) > 0 and len(docstr_lines[0].strip()) == 0:
            docstr_lines = docstr_lines[1:]
        # strip empty lines from the back
        while len(docstr_lines) > 0 and len(docstr_lines[-1].strip()) == 0:
            docstr_lines = docstr_lines[:-1]
        # recombine docstring without the function signatures
        docstr = '\n'.join(docstr_lines)
    
    # clean up docstring so we're not duplicating the overload data in each overloaded function
    if is_overloaded:
        docstr = strip_overloads_from_docstring(docstr)

    # strip out worthless docstrings like "return self<value" for __lt__
    if '\n' not in docstr.strip():
        line = (docstr.lower()
            .replace("return", "")
            .replace("self", "")
            .replace("value", "")
            .replace("name", "")
            .replace("implement", ""))
        for word in ("str", "repr", "hash", "setattr", "getattr"):
            if is_function(item) and word in item_name:
                line = line.replace(word, "")
        if not any(ch.isalnum() for ch in line):
            docstr = ''

    # replace invalid_idx literals with the "invalid_idx" constant
    docstr = docstr.replace(str(INVALID_IDX_CONSTANT), "invalid_idx")

    def fixup_type(ty: str) -> typing.Optional[str]:
        if ty == 'object':
            return None
        elif ty == "function":
            return "Callable"
        ty = ty.replace(f"{MODULE_NAME}.", "")
        ty = parse_cpp_type(ty)
        return ty

    # fixup sigs a bit
    forced_ret_type = None
    if item_name in BOOL_FUNC_NAMES:
        forced_ret_type = 'bool'
    elif item_name in INT_FUNC_NAMES:
        forced_ret_type = 'int'
    elif item_name in STR_FUNC_NAMES:
        forced_ret_type = 'str'
    elif 'ReturnType=Element' in docstr:
        #TODO: find a cleaner, less hard-coded way to encode this...
        docstr = docstr.replace('ReturnType=Element', '')
        forced_ret_type = 'Element'

    for sig in sigs:
        if forced_ret_type is not None:
            sig.ret_type = forced_ret_type
        elif sig.ret_type:
            if sig.ret_type == 'None':
                sig.ret_type = None
            else:
                sig.ret_type = fixup_type(sig.ret_type)
        if sig.args:
            for arg in sig.args:
                if arg and arg.arg_type:
                    arg.arg_type = fixup_type(arg.arg_type)

    return sigs, (docstr if not orig_docstring else item.__doc__)


def write_func(parent: typing.Any, fp: InterfaceFile, name: str, item: typing.Callable):
    sigs, docstr = parse_docstring(item, None, orig_docstring=DEBUG_DOCSTRINGS)

    func_is_method = False
    func_is_static = False
    if is_static_method(item):
        func_is_static = True
    if is_class(parent) or (len(sigs) > 0 and sigs[0] and len(sigs[0].args) > 0 and sigs[0].args[0] and sigs[0].args[0].name == 'self'):
        func_is_method = True
    
    func_is_static_method = func_is_method and func_is_static and not name.startswith("__")

    if sigs:
        for sig in sigs:
            if not sig.name:
                continue
            if sig.is_overload:
                fp.writeln("@typing.overload", indent=func_is_method)
            if func_is_static_method:
                fp.writeln("@staticmethod", indent=func_is_method)
            fp.write(f'def {sig.to_string(func_is_method)}:', indent=True)
            if docstr:
                fp.write('\n')
                fp.indent()
                fp.write_docstring(docstr)
                fp.dedent()
            else:
                fp.write(' ...\n')
            fp.writeln()
    else:
        arg_list = []
        ret_type = "typing.Any"

        if func_is_static_method:
            fp.writeln("@staticmethod", indent=func_is_method)
        else:
            arg_list.append("self")

        match name:
            case "__eq__" | "__ge__" | "__gt__" | "__le__" | "__lt__" | "__ne__":
                arg_list.append("rhs")
                if is_class(parent):
                    arg_list[-1] += f": {parent.__name__}"
                ret_type = "bool"
            case "__str__" | "__repr__":
                ret_type = "str"
            case "__setattr__" | "__setitem__":
                arg_list += ["name", "value"]
                ret_type = ''
            case "__hash__":
                ret_type = "int"
            case _:
                arg_list += ["*args", "**kwargs"]

        ret_type_sep = ' -> ' if len(ret_type) > 0 else ''
        fp.write(f'def {name}({(", ".join(arg_list))}){ret_type_sep}{ret_type}:', indent=True)
        if docstr:
            fp.write('\n')
            fp.indent()
            fp.write_docstring(docstr)
            fp.dedent()
        else:
            fp.write(' ...\n')
        fp.writeln()


def write_property(parent: typing.Any, fp: InterfaceFile, name: str, item: property):
    getter_sigs, getter_docstr = parse_docstring(item.fget, name)
    #print('getter_sigs', f'parent={parent!r}', name, getter_sigs, repr(getter_docstr))
    if len(getter_sigs) > 0:
        prop_type = getter_sigs[0].ret_type
    else:
        prop_type = 'typing.Any'

    fp.writeln("@property")
    fp.write(f"def {name}(self) -> {prop_type}:", indent=True)
    if getter_docstr:
        fp.write('\n')
        fp.indent()
        fp.write_docstring(getter_docstr)
        fp.dedent()
    else:
        fp.write(' ...\n')
    fp.writeln()

    if item.fset:
        fp.writeln(f"@{name}.setter")
        fp.write(f"def {name}(self, value: {prop_type}):", indent=True)
        setter_sigs, setter_docstr = parse_docstring(item.fset, name)
        if setter_docstr:
            fp.write('\n')
            fp.indent()
            fp.write_docstring(setter_docstr)
            fp.dedent()
        else:
            fp.write(' ...\n')
        fp.writeln()


def write_enum(parent: typing.Any, fp: InterfaceFile, name: str, enum_class: typing.Type):
    fp.writeln(f'class {name}(enum.Enum):')
    fp.indent()

    num_members = 0

    docstr = parse_docstring(enum_class)[1]
    if docstr:
        fp.write_docstring(docstr)
        num_members += 1

    # first collect enum names and values
    enum_values: list[tuple[str, int]] = []
    for value_name in dir(enum_class):
        if not hasattr(enum_class, value_name):
            continue
        enum_value = getattr(enum_class, value_name)
        if isinstance(enum_value, enum_class):
            value_int: int = int(getattr(enum_class, value_name))
            assert len(value_name) > 0
            # if a name is a reserved name in Python, add a trailing underscore
            if value_name in PYTHON_BUILTINS or value_name in PYTHON_KEYWORDS:
                value_name = f"{value_name}_"
            enum_values.append((value_name, value_int))
    
    # sort items by their int value, then write them
    enum_values.sort(key=lambda pair: pair[1])
    for name, val in enum_values:
        num_members += 1
        fp.writeln(f"{name} = {val}")

    if num_members == 0:
        fp.writeln('...')
    fp.dedent()
    fp.writeln()
    fp.writeln()


def write_class(parent: typing.Any, fp: InterfaceFile, name: str, item: typing.Type):
    fp.writeln(f'class {name}:')
    fp.indent()

    num_members = 0

    if docstr := parse_docstring(item)[1]:
        fp.write_docstring(docstr)
        num_members += 1
    
    for member_name in dir(item):
        if not hasattr(item, member_name) or member_name in ATTRIBUTES_BLACKLIST:
            continue
        num_members += 1
        write_item(item, fp, member_name, getattr(item, member_name))

    if num_members == 0:
        fp.writeln('...')
    fp.dedent()
    fp.writeln()
    fp.writeln()


def write_variable(parent: typing.Any, fp: InterfaceFile, name: str, item: typing.Any):
    if name == "__doc__":
        return
    elif name.startswith("__") and item is None:
        pass
    elif item is None:
        fp.writeln(f"{name}: None")
    elif isinstance(item, (int, float, str, bool)):
        fp.writeln(f"{name}: {type(item).__name__} = {item!r}")


def write_item(parent: typing.Any, fp: InterfaceFile, name: str, item: typing.Any):
    if item is None or isinstance(item, (int, float, str, bool)):
        write_variable(parent, fp, name, item)
    elif isinstance(item, property):
        write_property(parent, fp, name, item)
    elif is_enum(item):
        write_enum(parent, fp, name, item)
    elif is_class(item):
        write_class(parent, fp, name, item)
    elif is_function(item):
        write_func(parent, fp, name, item)
    else:
        fp.writeln(f"# [UNKNOWN TYPE {type(item).__name__}] {name!r} {item!r}")
        fp.write_docstring(parse_docstring(item)[1])
        fp.writeln()


if __name__ == "__main__":
    pyi = InterfaceFile(_script_args.output)
    try:
        pyi.writeln("# This file is autogenerated. Do not edit.")
        pyi.writeln("import typing")
        pyi.writeln("import enum")
        pyi.writeln("import datetime")
        pyi.writeln()

        for propname in dir(_pyjxc):
            if propname.startswith("__"):
                continue
            write_item(_pyjxc, pyi, propname, getattr(_pyjxc, propname))

    finally:
        pyi.close()

    print(f"Wrote Python interface stub to {_script_args.output!r}")

