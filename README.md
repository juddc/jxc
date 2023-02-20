![JXC Logo](https://raw.githubusercontent.com/juddc/jxc/main/media/jxc-logo-wide.svg)

* [JXC Documentation](https://jxc.juddnet.com/)
* [JXC Syntax Diagrams](https://jxc.juddnet.com/syntax_diagrams.html)

# What is JXC?

JXC is a structured data language similar to JSON, but with a focus on being expressive, extensible, and human-friendly. It's fantastic in many use-cases that can be awkward in other data formats. It's a perfect fit for config files, especially ones with more complex needs. It also does not sacrifice speed - benchmarks show that the reference implementation is about as fast as many commonly used C++ JSON libraries.

**Important Note**: JXC has not yet reached version 1.0. The current goal is to ship a 1.0 release in early 2023. **The 1.0 release will come with a strong commitment to backwards compatibility for the language syntax.** Along with this, the reference implementation will switch to semantic versioning - no breaking changes allowed except for major version bumps. The version numbers for the language and reference libraries will also likely be decoupled at this time, so they can evolve independently.

If you have feedback on the language syntax or semantics, now is the best time to offer them, while breaking changes are still allowed.

Until 1.0 is released, expect a few language and API changes.

## What does it look like?
```jxc
{
    # Comments!
    literal_types: [ null, true, false ]
    integers: [ -2, -1, 0, 1, 2, 4e6, 0xff, 0o644, 0b011011 ]
    floats: [ 3.141, -2.5, 1e-4, nan, +inf, -inf ]
    annotations: vec3[ 0.1, -0.4, 3.141 ]
    numeric_suffixes: [ 4px, 25%, 5mm, 22.3cm ]
    dates_and_datetimes: [ dt'2023-02-09', dt'2017-11-22T11:45:02Z' ]
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

# Core Language Tenets

- **Human Writable**. JXC allows single, double quoted, and multi-line raw strings. Comments are allowed. Unquoted identifiers may be used as object keys. In lists and objects, trailing commas are allowed, and line breaks may be used in place of commas if desired.

- **Human Readable**. Value annotations and numeric suffixes show the intended use of objects, lists, and numbers. To avoid ambiguity and maintain clarity, JXC does not allow unquoted strings as a normal value type. Indentation is not syntactically relevant.

- **Syntax/Parsing Flexibility**. Different applications have different needs. JXC is extensible via expressions, which you can parse yourself to support custom syntaxes. These can be useful for simple math expressions or even embedding snippets of scripting language code. If you want a data format that can elegantly support features such as `calc()` in CSS, you can build it with JXC.

# Core Features

- **JSON**. JXC is a superset of JSON. All valid JSON syntax is also valid JXC syntax. If you're already using JSON for your config files and want more flexibility, it's easy to migrate to JXC. If you find any valid JSON that JXC does not handle, please submit a bug report!

- **Numeric Suffixes**. JXC allows custom suffixes on numeric types, such as `10px`, `20.5%`, or `2.34_f32`. These suffixes can be evaluated at runtime and handled by your application however is needed, such as for conversion, storage, or data validation.

- **Annotations**. JXC allows annotations at the beginning of any value (eg. `vec3[0,0,1]`). The annotation syntax is flexible and you can use it for whatever purpose you need for your application - there are no built-in or reserved annotations. While their primary purpose is to store type information, they can also be used to store arbitrary metadata. This data can be read as a string, or, for more complex use-cases, it can be read as a list of tokens you can parse yourself (for example, to implement generics).

- **Expressions**. In addition to lists and objects, JXC has a third "container" type - expressions. These are tokenized but not parsed, so you can use expressions to effectively add custom syntax when needed. These pair very well with annotations, which you could use to select how to parse an expression.

- **Raw Strings**. Raw strings (`r'()'`) are strings that do not support escape characters, allowing a much greater range of possible values without dealing with escaping every single backslash or quote character. If you add a heredoc, they also allow line breaks for even greater flexibility (`r'HEREDOC()HEREDOC'`). Raw strings are excellent for regular expressions or embedding other languages.

- **DateTime Strings**. DateTime strings (`dt"2023-02-20"` or `dt"2023-02-20T10:40:05.025Z"`) are strings that only support ISO-8601 datetimes. Having a standard, first-class way to store dates and timestamps is incredibly useful for data such as log files or exported database tables. When using the Python bindings, these parse and serialize directly to datetime objects.

- **Base64 Strings**. JXC has first-class support for Base-64 encoded strings (`b64"anhjIGZvcm1hdA=="`) to make it easy to include binary data when needed. You can add parentheses to allow whitespace and line breaks anywhere to improve readability (`b64"(  anhjIGZvc  m1hdA==  )"`).

# Reference Implementation Details
The JXC reference implementation library has a two-stage parser. The first parsing stage handles JXC's syntax and yields a clear list of elements without allocating any memory for your data. The second-stage parser takes these elements and converts them into the appropriate data types, allocating memory however is needed for your environment. This means that language bindings are very efficient because each scripting language can have its own second-stage parser that allows that language to manage memory and data types appropriately. There are several second-stage parsers you can use depending on your use-case. If your application uses the C++ stdlib, you can use the C++ second-stage parser that uses a union type (`jxc::Value`) which can contain any valid JXC value as well as their associated annotations and numeric suffixes. If you have more complex needs (such as integrating with a game engine), writing a custom second-stage parser is not difficult (there is an example custom parser in `jxc_examples/src/custom_parser.cpp`).

# Python Usage
JXC includes first-class, fully featured Python bindings. It has two APIs - one that works similarly to `json.loads` and `json.dumps`, and a more flexible but verbose one.

## Parsing in Python
```python-repl
>>> import jxc
>>> print(jxc.loads("[1, 2, true, null, {}, dt'1999-07-18']"))
[1, 2, True, None, {}, datetime.date(1999, 7, 18)]
```

### Serializing in Python
```python-repl
>>> import jxc, datetime
>>> print(jxc.dumps([1, 2, True, None, {}, datetime.date(1999, 7, 18)]))
[1,2,true,null,{},dt"1999-07-18"]
```

# Editor Integration

## Visual Studio Code
Copy the directory `contrib/jxc.vscode` into the `extensions` directory in your Visual Studio Code user data directory (on Windows, this is `%USERPROFILE%/.vscode/extensions`).

## Sublime Text and Sublime Merge
Copy the directory `contrib/JXC.sublime-package` into the `Packages` directory in your Sublime Text user data directory (on Windows, this is `%APPDATA%/Sublime Text/Packages`). You can open this directory by opening the `Preferences` menu in Sublime Text and clicking `Browse Packages`.

Installing this package for Sublime Text will also enable it in Sublime Merge.
