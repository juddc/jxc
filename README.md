## What is JXC?

JXC is a structured data language similar to JSON, but with a focus on being expressive, extensible, and human-friendly. It's fantastic in many use-cases that can be awkward in other data formats. It's a perfect fit for config files, especially ones with more complex needs. It also does not sacrifice speed - benchmarks show that the reference implementation is about as fast as many commonly used C++ JSON libraries.

## Core tenets

- **Human writable**. JXC allows single, double quoted, and multi-line strings. Comments are allowed. Unquoted identifiers may be used as object keys. In lists and objects, trailing commas are allowed, and line breaks may be used in place of commas if desired.

- **Human readable**. Type annotations and numeric suffixes show the intended use of objects, lists, and numbers. To avoid ambiguity and maintain clarity, JXC does not allow unquoted strings as a normal value type. Indentation is not syntatically relevant.

- **Syntax flexiblity**. Different applications have different needs. JXC is intended to be extensible. It supports custom annotations on values, custom numeric suffixes, and custom expressions, all of which can be handled by custom parsers to store data in custom data types in whatever language you're using. All of these extensions should be syntactically unambiguous, and opt-in. From a performance point of view, you should only pay for what you use.

- **Parsing flexiblity**. This JXC library has a two-stage parser. The first parsing stage handles JXC's syntax and idioms and yields a clear list of elements without allocating any memory for your data. The second-stage parser takes these elements and converts them into the appropriate data types, allocating memory however is needed for your environment. This means that language bindings are very efficient because each scripting language can have its own second-stage parser that allows that language to manage memory and data types appropriately. There are several second-stage parsers you can use depending on your use-case. If your needs are relatively simple, there is a C++ second-stage parser that uses a union type (`jxc::Value`) that can contain any valid JXC value, along with its annotations and numeric suffixes. If you have more complex needs (such as integrating with a game engine), writing a custom second-stage parser is not difficult.

## Core features

- **JSON**. JXC is a superset of JSON. All valid JSON syntax is also valid JXC syntax. If you find any valid JSON that JXC does not handle, please submit a bug report!

- **Numeric suffixes**. JXC allows custom suffixes on numeric types, such as `10px`, `20.5%`, or `2.34_f32`. These suffixes can be evaluated at runtime and handled by your application however is needed, such as for conversion, storage, or, data validation.

- **Annotations**. JXC allows annotations at the beginning of any value, eg. `vec3[0,0,1]`. The annotation syntax is flexible and you can use it for whatever purpose you need for your application - there are no builtin annotations. While the primary purpose is to store type information, they could also be used to store arbitrary metadata. This data can be read as a string, or, for more complex use-cases, it can be read as a list of tokens for use by a custom parser (for example, to implement generics).

- **Expressions**. In addition to lists and objects, JXC adds a third "container" type called expressions. These are parsed as a list of tokens for your application to handle however is needed. These pair very well with annotations, which you could use to select how to parse an expression.

- **Raw strings**. Raw strings (`r'()'`) are strings without any escape characters, allowing a much greater range of possible values without dealing with escaping every single backslash or quote character. These are multi-line strings that support the heredoc syntax for even greater flexibility (`r'HEREDOC()HEREDOC'`). Raw strings are excellent for regular expressions or embedded scripting language snippets.

- **Hexbyte strings**. JXC has support for hexadecimal strings in the form `bx"aaff00"` that can return byte arrays. If you add parentheses they also support whitespace, including line breaks (`bx"( aa ff 00 )"`).

- **Base64 strings**. JXC has first-class support for base64 (`b64"anhjIGZvcm1hdA=="`) to make it easy to include binary data when needed. Like hexbyte strings, you can add parentheses to be able to add whitespace and linebreaks anywhere (`b64"(  anhjIGZvc  m1hdA==  )"`)


## What does it look like?

- **Base value types**:
  * Null: `null`
  * Bool: `true`, `false`
  * Numbers (int, float, hex, octal, binary): `123`, `1.234`, `0xff`, `0o777`, `0b1011`
  * Strings: `"abc"`, `'abc'`
  * Raw strings: `r"(abc)"`, `r"HEREDOC(multi-line string)HEREDOC"`
  * Hex-byte strings: `bx''`, `bx"00ff"`, `bx"( 00 ff )"`
  * Base64-byte strings: `b64''`, `b64"/xEiu/8="`, `b64"( /xE iu/8= )"`
  * Lists: `[]`, `[0, 1, 2]`, `[true, null, 1.25rem]`
  * Objects: `{}`, `{"x": 0, "y": 2, "z": 3}`, `{x: 0, y: 2, z: 3}`
  * Expressions: `()`

- **Number suffixes**:
  * Number suffixes can be 1-15 characters and contain only `a-z`, `A-Z`, `_`, or `%` (characters after the first can also be `0-9`)
  * `10px`
  * `20.5%`
  * `0xFFpx`
  * `0o677_perm`

- **Annotations**:
  * Annotations must start with a identifier:
  * `vec3{ x: 0, y: 0, z: 1 }`
  * `rgb[255, 255, 127]`
  * `rgb bx'ffffcc'` (NB. Single-value types must have a space between the annotation and the value. Container types can omit the space.)
  * `hsva{ h: 1.0, s: 0.5, v: 0.5, a: 0.95 }`
  * `box{ top: 20%, left: 25%, width: 200px, height: 200px }`
  * Multiple identifiers can be used if you separate them with dots:
  * `std.vector[ 0, 1, 2, 3, 4, 5 ]`
  * Annotations may end with a single angle-bracket pair `<>`. Inside you can use identifiers and some operators.
  * `vec3<float>{ x: 1.0, y: 2.0, z: 3.0 }`
  * `std.vector<int32_t>[ 100, 101, 102 ]`
  * `std.unordered_map<std.string, std.vector<int32_t>>{ a: [1, 2, 3], b: [4, 5, 6] }`
  * `list<int | float | str>[ 0, 1, -5.12, "abc" ]`
  * Inside angle brackets, you can also use the non-container value types null, bool, integer, float, and string:
  * `fixed_array<int32_t, 8, FixedSize=true>[ 0, 1, 2, 3, 4, 5, 6, 7 ]`
  * Annotations may start with an exclamation point. What this indicates is up to you. You could use it for tags such as `!important` in CSS, to disable a value from being used, or to invert a condition of some kind.
  * `!vec3<float>{ x: 1.0, y: 2.0, z: 3.0 }`
  
- **Object keys**:
  * There are several types of valid object keys:
  * Null or Bool: `{ null: 123, true: 456, false: 789 }`
  * Integers (numeric suffixes not allowed): `{ 0: 'abc', 1: 'def', 432: 'ghi' }`
  * Quoted strings: `{ "abc": [], "def": [] }`
  * Unquoted identifiers: `{ abc: [], def: [], $abc: [], abc$: [], $abc$: [] }`
  * Unquoted dotted identifiers: `{ abc.def: [], aaa.bbb.ccc.ddd.eee.fff: null }`
  * Unquoted dashed identifiers: `{ abc-def: [], aaa-bbb-ccc-ddd-eee-fff: null }`
  * Asterisks: `{ *: [], *.*: [], *.abc.*: [] }`

- **Expressions**:
  * Expressions allow arbitrary identifiers, operators, and non-container values, and are generally parsed by the application.
  * `()`
  * `(0)`
  * `(null)`
  * `(1 + 2 - 3)` (parsed as `[1, '+', 2, '-', 3]`)
  * `(a, b, c=5)` (parsed as `['a', ',', 'b', ',', 'c', '=', 5]`)
  * `(40px + 20%)`
  * `((5 < 3) or (3 > -1))`
  * `(FileType "/a/b/asset.ext")`
  * Expressions are particularly useful when paired with annotations, as you can use the annotation to determine how to parse the expression.
  * `python((x**2) + (3 * x) - abs(x))`
  * `asset(FileType "/a/b/asset.ext")`
  * `ref(Core.Asset.AssetName)`
  * One useful technique is using expressions for enums, as you can use the enum value name inside parentheses instead of quoting it as a string.
  * Given the C++ enum `enum class EnumName : uint8_t { FirstValue = 0, SecondValue, ThirdValue };`, you could serialize values as `EnumName(SecondValue)`.

## Examples


## Editor Integration

### Visual Studio Code
Copy the directory `contrib/jxc.vscode/` into the `extensions` directory in your Visual Studio Code user data directory (on Windows, this is `%USERPROFILE%/.vscode/extensions`).

### Sublime Text and Sublime Merge
Copy the directory `contrib/JXC.sublime-package` into the `Packages` directory in your Sublime Text user data directory (on Windows, this is `%APPDATA%/Sublime Text/Packages`). You can open this directory by opening the `Preferences` menu in Sublime Text and clicking `Browse Packages`.

Installing this package for Sublime Text will also enable it in Sublime Merge.
