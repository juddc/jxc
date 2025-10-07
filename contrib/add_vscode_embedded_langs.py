#!/usr/bin/env python3
"""
Visual Studio Code's language syntax defintions do not seem to support a reusable way to use other language's
syntax highlighters. Rather than deal with a lot of copy-pasting, this script generates the relevant matchers
for each supported language.

This script only needs to be run when adding or changing the list of supported languages.
"""
import json

TMLANG_PATH = './jxc.vscode/jxc.tmLanguage.json'

LANGUAGES = [
    { 'name': 'bat', 'alt_names': [], 'include': 'source.batchfile' },
    { 'name': 'c', 'alt_names': ['clang'], 'include': 'source.c' },
    { 'name': 'cpp', 'alt_names': [], 'include': 'source.cpp' },
    { 'name': 'csharp', 'alt_names': ['cs'], 'include': 'source.cs' },
    { 'name': 'css', 'alt_names': [], 'include': 'source.css' },
    { 'name': 'go', 'alt_names': [], 'include': 'source.go' },
    { 'name': 'html', 'alt_names': [], 'include': 'text.html.basic' },
    { 'name': 'ini', 'alt_names': [], 'include': 'source.ini' },
    { 'name': 'java', 'alt_names': [], 'include': 'source.java' },
    { 'name': 'js', 'alt_names': ['javascript'], 'include': 'source.js' },
    { 'name': 'json', 'alt_names': [], 'include': 'source.json' },
    { 'name': 'lua', 'alt_names': [], 'include': 'source.lua' },
    { 'name': 'markdown', 'alt_names': ['md'], 'include': 'text.html.markdown' },
    { 'name': 'perl', 'alt_names': [], 'include': 'source.perl' },
    { 'name': 'php', 'alt_names': [], 'include': 'source.php' },
    { 'name': 'phphtml', 'alt_names': [], 'include': 'text.html.php' },
    { 'name': 'powershell', 'alt_names': [], 'include': 'source.powershell' },
    { 'name': 'python', 'alt_names': ['py'], 'include': 'source.python' },
    { 'name': 'regexp', 'alt_names': [], 'include': 'string.regexp' },
    { 'name': 'ruby', 'alt_names': [], 'include': 'source.ruby' },
    { 'name': 'rust', 'alt_names': [], 'include': 'source.rust' },
    { 'name': 'shell', 'alt_names': ['bash'], 'include': 'source.shell' },
    { 'name': 'sql', 'alt_names': [], 'include': 'source.sql' },
    { 'name': 'ts', 'alt_names': ['typescript'], 'include': 'source.ts' },
    { 'name': 'vb', 'alt_names': [], 'include': 'source.asp.vb.net' },
    { 'name': 'xml', 'alt_names': [], 'include': 'text.xml' },
    { 'name': 'yaml', 'alt_names': [], 'include': 'source.yaml' },
]

EMBEDDED_LANGUAGE_MULTI_LINE_STRING_TEMPLATE = r"""
{
    "begin": "(r)(['\"])(%HEREDOC_MATCHES%)(\\()",
    "end": "(\\))(\\3)(\\2)",
    "name": "string.quoted.raw.heredoc.%LANG_NAME%.jxc",
    "contentName": "string.quoted.raw.heredoc.%LANG_NAME%.jxc",
    "beginCaptures": {
        "1": { "name": "storage.type.string.raw.jxc" },
        "2": { "name": "storage.type.string.quote.jxc" },
        "3": { "name": "entity.name.tag.heredoc.jxc" },
        "4": { "name": "storage.type.string.parens.jxc" }
    },
    "endCaptures": {
        "1": { "name": "storage.type.string.parens.jxc" },
        "2": { "name": "entity.name.tag.heredoc.jxc" },
        "3": { "name": "storage.type.string.quote.jxc" }
    },
    "patterns": [
        { "include": "%LANG_INCLUDE%" }
    ]
}
"""

RAW_MULTI_LINE_STRING = r"""
{
    "begin": "(r)(['\"])([^\\(\\)\\\\ ]{0,16})(\\()",
    "end": "(\\))(\\3)(\\2)",
    "name": "string.quoted.raw.jxc",
    "beginCaptures": {
        "1": { "name": "storage.type.string.raw.jxc" },
        "2": { "name": "storage.type.string.quote.jxc" },
        "3": { "name": "constant.character.escape.jxc" },
        "4": { "name": "storage.type.string.parens.jxc" },
        "5": { "name": "string.quoted.raw.jxc" }
    },
    "endCaptures": {
        "1": { "name": "storage.type.string.parens.jxc" },
        "2": { "name": "constant.character.escape.jxc" },
        "3": { "name": "storage.type.string.quote.jxc" }
    }
}
"""


def make_language_pattern(name: str, alt_names: list[str], lang_include: str) -> dict:
    pat: str = EMBEDDED_LANGUAGE_MULTI_LINE_STRING_TEMPLATE
    heredoc_strings: list[str] = [ name ] + alt_names
    pat = pat.replace("%HEREDOC_MATCHES%", '|'.join(heredoc_strings))
    pat = pat.replace("%LANG_NAME%", name)
    pat = pat.replace("%LANG_INCLUDE%", lang_include)
    return json.loads(pat)

with open(TMLANG_PATH, 'r') as fp:
    defs = json.load(fp)

patterns: list = defs["repository"]["string_raw_multi_line"]["patterns"]
assert isinstance(patterns, list)

patterns.clear()

# Add all languages
for lang in LANGUAGES:
    patterns.append(make_language_pattern(lang['name'], lang['alt_names'], lang['include']))

# Add the base case for when the language doesn't match
patterns.append(json.loads(RAW_MULTI_LINE_STRING))

# Write the file back out
with open(TMLANG_PATH, 'w') as fp:
    json.dump(defs, fp, indent=4)

