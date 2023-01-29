# Introduction

JXC is a structured data language similar to JSON, but with a focus on being expressive, extensible, and human-friendly. It's fantastic in many use-cases that can be awkward in other data formats. It's a perfect fit for config files, especially ones with more complex needs. It also does not sacrifice speed - benchmarks show that the reference implementation is about as fast as many commonly used C++ JSON libraries.

What does it look like?

```jxc
{
    # Comments!
    literal_types: [ null, true, false ]
    integers: [ -2, -1, 0, 1, 2, 4e6, 0xff, 0o644, 0b011011 ]
    floats: [ 3.141, -2.5, 1e-4 ]
    annotations: vec3[ 0.1, -0.4, 3.141 ]
    numeric_suffixes: [ 4px, 25%, 5mm, 22.3cm ]
    strings: [
        'Single-quoted string'
        "Double-quoted string"
    ]
    raw_strings: r"HEREDOC(
        Raw strings, which support standard heredoc syntax
        so you don't need to worry about escaping anything
    )HEREDOC"
    base64_strings: b64'anhjIGZvcm1hdA=='
}
```

JXC looks similar to JSON - that's by design. JXC is actually a super-set of JSON, so all valid JSON is valid JXC. If you're already using JSON for your config files, switching is trivial.

Unlike YAML, JXC does not care about indentation. Use whatever indentation scheme you like.

# Syntax

## Base Value Types
All value types supported by JXC

* **Null**
    - `null`
* **Booleans**
    - `true`
    - `false`
* **Numbers**
    - `123` (decimal integers)
    - `1.234` (floating point literals)
    - `2e4` (exponent-based integers)
    - `1.1e-3` (exponent-based floating point values)
    - `0xFF` (hexadecimal integers)
    - `0o777` (octal integers)
    - `0b1011` (binary integers)
* **Strings** (single and double quotes are interchangeable)
    - `"abc"`
    - `'abc'`
    * String escapes: JXC supports the same set of escape types that Python supports, which itself is a super-set of what JSON supports. Notably, JXC and Python support utf32 escapes, which JSON lacks.
        - `" \n \t \xFF \u2192 \U0001F601 "`
* **Raw strings**
    - Raw strings always start and end with an inner set of parentheses. The heredoc is optional for single-line strings.
    - `r"()"`
    - `r"([a-zA-Z0-9\W\s]*)"`
    - `r"HEREDOC(multi-line string)HEREDOC"`
    - `r"CUSTOM_HEREDOC(multi-line string)CUSTOM_HEREDOC"`
* **Base64 strings**
    - `b64''`
    - `b64"/xEiu/8="`
    - Base64 strings that start and end with an inner set of parentheses can contain arbitrary whitespace and line breaks.
    - `b64"( /xE iu/8= )"`
* **Lists**
    - `[]`
    - `[0, 1, 2]`
    - `[true, null, 1.25rem]`
* **Objects**
    - `{}`
    - `{"x": 0, "y": 2, "z": 3}`
    - `{x: 0, y: 2, z: 3}`
* **Expressions**
    - `()`
    - `(1 + 2)`
    - `(unquoted_identifier = 24px + 17%, "string value")`

## Container Types

JXC has three container types - lists, objects, and expressions.

Lists use square brackets (`[` `]`) and contain zero or more values separated by either commas or line breaks.

Objects use curly braces (`{` `}`) and contain zero or more key-value pairs separated by either commas or line breaks, with a colon `:` after each key and before each value.

Expressions use parentheses (`(` `)`) and are a list of arbitrary tokens and may only contain identifiers, operators, and single values (null, booleans, numbers, and strings).

### A note on commas

While JSON has very strict rules around commas, JXC is more relaxed. Arrays and objects allow using either a comma or a line break as a value separator. While you can use a comma after every value in a container, the recommended practice for human-edited JXC files is to use commas only when you want to place multiple values on a single line, or when it would help readability.

## Number Suffixes
If you need to support different kinds of numeric values, JXC support numeric suffixes. You can handle these however you need - converting the values at parse time, wrapping them in a value+enum structure, or even just using them for validation and erroring out if a value has a missing or unexpected suffix.

```jxc
{
    width: 50%
    height: 25.7px
    angle: 47.002deg
}
```

Number suffixes can be 1-15 characters long. The first character must be `a-z`, `A-Z`, or `%`, and all characters past the first can also contain `0-9` or `_`. Note that JXC allows a leading underscore - this underscore is not part of the actual suffix, it's just used as a visual separator, so `12px` and `12_px` have the same value and suffix. For hexadecimal values, this leading underscore is required to avoid ambiguity.

More examples:

```jxc
10in
20.52%
0xFF_px
0o677_perm
```

## Annotations
JXC can be customized for many use cases that are difficult or unwieldy in JSON. For example, type information is commonly added to (well, shoehorned into) JSON objects. In JXC you get this for free - JXC has first-class support for type annotations. Annotations must start with an identifier. You can use dots (`.`) to add multiple identifiers, eg. `std.vector`.

One common JSON idiom is using objects with a key to store the object's type, for example:
```json
{ "$type": "some.library.type", "x": 5, "y": 10 }
```

In JXC this is a first-class language feature:
```jxc
some.library.type{ x: 5, y: 10 }
```

You can even go further with it and use annotations as a way to normalize data, allowing a large amount of flexibility in how users can enter values. By using annotations, you could automatically convert any value that has a given annotation to its intended type. This is also useful for data validation.

```jxc
vec3{x: 1, y: 2, z: 3}
vec3[-3, -2, -1]
vec3 0.2
vec3 null
```

More annotation examples:
```jxc
hsva{ h: 1.0, s: 0.5, v: 0.5, a: 0.95 }
box{ top: 20%, left: 25%, width: 200px, height: 200px }
```

Single-value types must have a space between the annotation and the value. Container types may omit the space.
```jxc
rgb[255, 255, 127]  # space optional
rgb 0xFF9ACC        # space required
```

Multiple identifiers can be used if you separate them with dots:
```jxc
std.vector[ 0, 1, 2, 3, 4, 5 ]
```

Annotations may start with an exclamation point. What this indicates is up to you. You could use it as a directive of some kind, for tags (such as how `!important` is used in CSS), to disable a value from being used, to invert a condition of some kind.
```jxc
!include "path/to/abc.jxc"
!override.settings{ listen.host: 'localhost', listen.port: 8080 }
```

Annotations may end with a single angle-bracket pair `<` `>`. Inside these angle brackets you can use identifiers and some operators.
```jxc
vec3<float>{ x: 1.0, y: 2.0, z: 3.0 }
std.vector<int32_t>[ 100, 101, 102 ]
std.unordered_map<std.string, vec3<int32_t>>{ 'a': [1, 2, 3], 'b': [4, 5, 6] }
list<int | float | str>[ 0, 1, -5.12, "abc" ]
```

Inside an annotation's angle brackets, you can also use the non-container value types: null, bool, integer, float, and string
```jxc
fixed_array<int32_t, 8, FixedSize=true, name="numbers", meta=null>[]
```

## Objects
There are several types of valid object keys - null, bool, integer literals, string literals, as well as unquoted identifiers. These identifiers can contain alphanumeric characters, `_`, `$`, and `*`. You may also use multiple identifiers separated by dots (`.`).

(Specifically, unquoted object key identifiers must match the regular expression `[a-zA-Z_$*][a-zA-Z0-9_$*]*`).

Null or boolean literals as object keys:
```jxc
{ null: 123, true: 456, false: 789 }
```

Integer object keys (numeric suffixes are not allowed):
```jxc
{ 0: 'abc', 1: 'def', 432: 'ghi', -7: 'xyz' }
```

Quoted strings:
```jxc
{ "abc": [], 'def': [] }
```

Unquoted identifiers:
```jxc
{ abc: [], def: [], $abc: [], abc$: [], $abc$: [] }
```

Unquoted dotted identifiers:
```jxc
{ abc.def: [], aaa.bbb.ccc.ddd.eee.fff: null }
```

Asterisks:
```jxc
{ *: [], *.*: [], *.abc.*: [] }
```

## Expressions

If you have data that does not fit neatly into the limited set of data structures that languages like JSON support, and you don't mind getting your hands dirty writing a small amount of parsing code, you can use expressions to support whatever kind of syntax you need.

Expressions are effectively a list of unparsed tokens, and you can read expressions however you need to - either as a sequence of tokens, or as a string containing the entire expression (for example, to eval it in a scripting language). Note that unlike arrays and objects, expressions do *not* support nested containers. This is because JXC's parser does not parse the contents of expressions - that's the whole point. They can contain identifiers, strings, numbers, booleans, null, and a reasonable list of common operators.

```jxc
# Expressions use parentheses instead of square brackets or curly braces.
# This is an empty expression:
()

# This can be parsed as the array [Number(1), Operator('+'), Number(1)],
# or as the string "1 + 1".
(1 + 1)

# Annotations pair very naturally with expressions - you can use them to
# select what parsing function to use.
python_eval(value + 2 if (value / 2 > 4) else value - 2)

# You could use expressions to express constraints for a particular system
bind_function(name 'do_stuff', int32 'x' > 0, int32 'y' > 0)
```

More examples:
```jxc
()
(0)
(null)
(1 + 2 - 3)  # (parsed as [1, '+', 2, '-', 3])
(a, b, c=5)  # (parsed as ['a', ',', 'b', ',', 'c', '=', 5])
(40px + 20%)
((5 < 3) or (3 > -1))
(FileType "/a/b/asset.ext")
asset(FileType "/a/b/asset.ext")
python((x**2) + (3 * x) - abs(x))
ref(Core.Asset.AssetName)
```

One useful technique is using expressions for enums, as you can use the enum value name inside parentheses as an unquoted identifier instead of quoting it as a string. For example, given this C++ enum type:
```cpp
enum class EnumName : uint8_t
{
    FirstValue = 0,
    SecondValue,
    ThirdValue,
};
```

You could serialize `EnumName` values as:
```jxc
EnumName(SecondValue)
```

An ideal use of expressions is when you need to treat math formulas as data. For example, if you wanted to define an RPG stat system.
```jxc
{
    characters: {
        'hero': Character{ attack: 12, defense: 10 }
        'enemy': Character{ attack: 8, defense: 6 }
    }
    weapons: [
        'sword': Formula((self.attack * 1.3) - (target.defense * 0.9))
        'axe': Formula((self.attack * 1.4) - target.defense)
    ]
}
```

Expressions can contain line breaks and comments as well, so they're a great fit for putting a small amount of logic into your data, without the language itself dictating how that logic should be used.

```jxc
# This is a valid expression. It also happens to be valid Lua syntax.
(
    local i = 0
    while (i < 10) do
        print(i)
        i += 1
    end
)
```

This is just an example of how flexible the expression syntax is. It's not recommended to write whole programs in expressions, or even very complex logic - you're not going to have a great time debugging that.

That said, JXC can be an excellent choice for simple [DSLs](https://www.wikipedia.com/wiki/Domain-specific_language). If you wanted a small, non-turing complete scripting system that just allows users to run a sequence of functions, that's an easy task for expressions.
```jxc
[
    set_environment_variable("DEBUG_MODE" = 1)
    launch_game[ "game.exe", "--flag1", "--flag2" ]
    init_renderer(os == "nt" ? "directx12" : "vulkan")
    set_graphics_mode(fullscreen, 1920, 1080)
    enable_physics_debug(true)
    run_physics_tests(require: intersection_depth >= -0.0001)
    upload_physics_test_results_to_server("127.0.0.1:8080")
    exit()
]
```
