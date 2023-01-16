# Introduction

JXC is a structured data language similar to JSON, but with a focus on being expressive, extensible, and human-friendly. It's fantastic in many use-cases that can be awkward in other data formats. It's a perfect fit for config files, especially ones with more complex needs. It also does not sacrifice speed - benchmarks show that the reference implementation is about as fast as many commonly used C++ JSON libraries.

What does it look like?

```jxc
{
    # Comments!
    literal_types: [ null, true, false ]
    integers: [ -2, -1, 0, 1, 2, 4e6 ]
    floats: [ 3.141, -2.5, 1e-4 ]
    hex_oct_and_binary: [ 0xff, 0o644, 0b011011 ]
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

JXC looks similar to JSON - that's by design. JXC is actually a superset of JSON, so all valid JSON is valid JXC. If you're already using JSON for your config files, switching is trivial.

Unlike YAML, JXC does not care about indentation. Use whatever indentation scheme you like.

### A note on commas
While JSON has very strict rules around commas, JXC is more relaxed. Arrays and objects allow using either a comma or a line break as a value separator. While you can use a comma after every value in a container, the recommended practice is to use commas only when you want to place multiple values on a single line, or when it would help readability.

## Extensibility

### Type Annotations
JXC can be customized for many use cases that are difficult or unwieldy in JSON. For example, type information is commonly added to (well, shoehorned into) JSON objects. In JXC you get this for free - JXC has first-class support for type annotations.

One common JSON idiom is using objects with a `$type` key, for example:
```json
{ "$type": "some.library.type", "x": 5, "y": 10 }
```

In JXC this is a first-class language feature:
```jxc
some.library.type{ x: 5, y: 10 }
```

You can even go further with it and use annotations as a way to normalize data, allowing a large amount of flexibility in how users can enter values. By using annotations, you could automatically convert any value (or error out) that has a given annotation to its intended type.

```jxc
[
    vec3{x: 1, y: 2, z: 3}
    vec3[-3, -2, -1]
    vec3 0.2
    vec3 null
]
```

### Numeric Suffixes
If you need to support different kinds of numeric values, JXC support numeric suffixes. You can handle these however you need - converting the values at parse time, wrapping them in a value+enum structure, or even just using them for validation and erroring out if a value has a missing or incorrect suffix.

```jxc
{
    width: 50%
    height: 25.7px
    angle: 47.002deg
}
```

### Expressions

Lastly, if you have data that does not fit neatly into the limited set of data structures that languages like JSON support, and you don't mind getting your hands dirty writing a small amount of parsing code, you can use expressions to support whatever kind of syntax you need.

Expressions are effectively a list of unparsed tokens, and you can read expressions however you need to - either as a sequence of tokens, or as a string containing the entire expression (for example, to eval it in a scripting language). Note that unlike arrays and objects, expressions do *not* support nested containers. This is because JXC's parser does not parse the contents of expressions - that's the whole point. They can contain identifiers, strings, numbers, bools, null, and a reasonable list of common operators.

```jxc
[
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
    bind_function(name "do_stuff", int32 'x' > 0, int32 'y' > 0)
]
```

An ideal use of expressions is when you need to treat math formulas as data. For example, if you wanted to define an RPG stat system.
```jxc
{
    characters: [
        Character{
            name: 'Hero'
            stats: { attack: 12, defense: 10 }
        }
    ]
    weapons: [
        sword: Formula((self.attack * 1.3) - (target.defense * 0.9))
        axe: Formula((self.attack * 1.4) - target.defense)
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

This is just an example of how flexible the expression syntax is. It's not recommended to write whole programs using expressions, or even very complex logic - you're not going to have a great time debugging that.

That said, JXC can be an excellent choice for simple DSLs (domain-specific languages). If you wanted a small, non-turing complete scripting system that just allows users to run a sequence of functions, that's an easy task for expressions.
```jxc
[
    set_environment_variable("DEBUG_MODE" = 1)
    launch_game[ "game.exe", "--flag1", "--flag2" ]
    init_renderer("directx12")
    set_graphics_mode(fullscreen, 1920, 1080)
    enable_physics_debug(true)
    run_physics_tests(intersection_depth > 0.0001)
    upload_physics_test_results_to_server("127.0.0.1:8080")
    exit()
]
```
