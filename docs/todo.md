# Open Design Questions
These need a definitive answer before a 1.0 release.


## Object key identifiers vs regular identifiers
* Currently, JXC uses two different identifier types - one for object keys, and one for annotations and expressions.
* The only difference is that object keys support `*` characters, for use in wildcard-style syntax, eg.
    - `{ *: true, *.abc.*: false, *abc*.*: null }`
* Is it worth the extra parsing complexity (and, to a lesser extent, the extra learning curve) to support this feature?


## Expression operators and negative numeric values
* Currently, the expression parser treats all operators (including all instances of `-`) as individual tokens. 
* This means that while you can use value types such as bools and strings in expressions without issue, numbers have a bit of an unexpected footgun.
    - The expression `(-5)` returns the tokens [`Minus(-)`, `Number(5)`], not `Number(-5)` as a user may expect.
    - This is compounded in more complex math expressions such as `(2 - -5)`, which returns the tokens [`Number(2)`, `Minus(-)`, `Minus(-)`, `Number(5)`].
    - Is there an elegant solution that allows users who _want_ all raw tokens for custom handling to get that, while users that expect [`Number(2)`, `Minus(-)`, `Number(-5)`] to get that?
    - Presumably, if such a feature existed, it would need to be per-expression.


## Add Python-style byte strings?
* Would work exactly like base64 strings (same internal storage as an array of `uint8_t`), but more readable.
* Ex. `b"abc\n\xFF\x00\x4aALLOWS_ascii_CHARS"`
