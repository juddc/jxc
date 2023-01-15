# JXC Core Library

JXC's reference implementation is split up into two libraries - `libjxc` and `libjxc_cpp`.

The core library (`libjxc`) contains the lexer and a first-stage parser that attempts to be memory and storage agnostic, where possible. The idea is that it handles all the syntax implementation details, but does not allocate memory for values - that's up to a second-stage parser. The core library provides utility functions to make this easier. The reasoning for this is that different environments use different data structures, and trying to handle them all with C++ templates can be painful at best. In this way, we can reuse the same lexer and first-stage parser for other environments (for example, script language bindings) without excess copying of data in an intermediate data structure.

The C++ library (`libjxc_cpp`) is a second-stage parser that's built around the C++ standard library. It's optional - if, for example, you're integrating JXC into an Unreal Engine game project, you can write your own second-stage parser that parses directly to types like `FString` rather than `std::string`.

The rest of this document explains how to use the core library - if you want a simpler API built around the C++ standard library, see the documentation for the [JXC C++ Library](jxc_cpp_library.html).

## Parsing

`JumpParser` is a first-stage parser that returns Elements. These elements are intended to be as simple as possible to parse. For example, after getting a `BeginArray` element, every element that follows is a value in that array until it returns an `EndArray` element.

```c++
#include <iostream>
#include <string>
#include "jxc/jxc.h"

int main(int argc, const char** argv)
{
    std::string jxc_string = "[1, 2, true, null, 'string']";
    jxc::JumpParser parser(jxc_string);
    while (parser.next())
    {
        const jxc::Element& value = parser.value();
        std::cout << value.to_repr() << '\n';
    }
    if (parser.has_error())
    {
        std::cerr << "Parse error: " << parser.get_error().to_string() << '\n';
    }
    return 0;
}
```

This program outputs the following:
```
Element.BeginArray
Element.Number(Token.Number("1"))
Element.Number(Token.Number("2"))
Element.Bool(Token.True("true"))
Element.Null
Element.String(Token.String("'string'"))
Element.EndArray
```

## Serialization
`Serializer` is a serializer type that outputs JXC to a buffer. Note that you can provide your own output buffer type that converts directly to a custom string type.

```c++
#include <iostream>
#include "jxc/jxc.h"

int main(int argc, const char** argv)
{
    jxc::StringOutputBuffer buffer;
    jxc::Serializer doc(&buffer);
    doc.array_begin()
        .value_int(1)
        .value_int(2)
        .value_bool(true)
        .value_null()
        .value_string("string")
        .array_end();
    doc.flush();
    std::cout << buffer.to_string() << '\n';
    return 0;
}
```

This program outputs the following:
```jxc
[
    1,
    2,
    true,
    null,
    "string",
]
```
