# Open Design Questions
These need a definitive answer before a 1.0 release.


## Object key identifiers vs regular identifiers
* Currently, JXC uses two different identifier types - one for object keys, and one for annotations and expressions.
* The only difference is that object keys support `*` characters, for use in wildcard-style syntax, eg.
    - `{ *: true, *.abc.*: false, *abc*.*: null }`
* Is it worth the extra parsing complexity (and, to a lesser extent, the extra the learning curve) to support this feature?


## Expression operators and negative numeric values
* Currently, the expression parser treats all operators as individual tokens. 
* This means that while you can use value types such as bools and strings in expressions without issue, numbers have a bit of an unexpected footgun.
    - The expression `(-5)` returns the tokens [`-`, `5`], not `-5` as a user may expect.
    - This is compounded in more complex math expressions such as `(2 - -5)`, which returns the tokens [`2`, `-`, `-`, `5`].
    - Need to find an elegant solution to this so that users who _want_ all raw tokens for custom handling can get that, while users that expect [`2`, `-`, `-5`] get that.


## Remove hexbyte type
* Doesn't seem like this would get much use
* Possibly replace with a Python-style bytes type that works like a string but converts to a byte array?


## Add native datetime type
* While annotations make these easy enough, having a standard might be a good idea.
    - Proposal: new string prefix `dt` that only accepts ISO-8601 (or a subset of ISO-8601) dates.
        - `dt"2023-01-15T00:00:42Z"`
        - This has the advantage of being easily convertable to a string for any system that does not have a standard datetime type, while maintaining the benefit of having the values be validated at parse time.


## Add native support for NaN, +Inf, and -Inf
* This is easily supportable with annotations and expressions, but it would be nice to have a standard way to do this for interoperability.
- JXC has no reserved annotation types, so for this to make sense it would need to have a syntax that's unambigious
- JXC only has three reserved multi-character tokens (`null`, `true`, and `false`), and it seems odd to reserve more, especially for values that are much more rarely used
- Need to bikeshed this more. Possible syntaxes:
    - Require a sign character on infinity, so that we're only adding a single new identifier (`nan`).
        - `+inf`
        - `-inf`
        - `nan`
            - One possible footgun with this approach is object keys. If `nan` is used as an unquoted object key, is that a parse error, or should it treat `nan` as an identifier in that case? Note that floats are already not valid object keys. This could create a small amount of ambiguity.
    - Use a number prefix (like the hex/oct/bin prefixes) so the constants always get parsed by the existing number parser. Trivial to parse, but reads poorly.
        - `0f.nan` or `0fnan`
        - `+0f.inf` or `+0finf`
        - `-0f.inf` or `-0finf`
    - Add a new string prefix. Reads a little odd, and the natural choice, `f`, looks too similar to Python's f-strings.
        - `f"nan"`
        - `f"+inf"`
        - `f"-inf"`
    - Add a new quote character specifically for language-reserved constants. This has the benefit of allowing more constants to be added in the future without conflicting with user data, but is more verbose than it could be.
        - ``` `nan` ```
        - ``` `+inf` ```
        - ``` `-inf` ```
