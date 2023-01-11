## Open Design Questions
These need a definitive answer before a 1.0 release.


### Should JXC support `_` separators in numeric literals?
* Adding this could cause parsing ambiguity with numeric suffixes.
* One existing syntax quirk is that numeric suffixes can optionally begin with an underscore, but for hex literals this underscore is required to distinguish between hex digits and the numeric suffix.


### Object key identifiers vs regular identifiers
* Currently, JXC uses two different identifier types - one for object keys, and one for annotations and expressions.
* The only difference is that object keys support `*` characters, for use in wildcard-style syntax, eg.
    - `{ *: true, *.abc.*: false, *abc*.*: null }`
* Is it worth the extra parsing complexity (and, to a lesser extent, the extra the learning curve) to support this feature?


### Expression operators and numeric values
* Currently, the expression parser treats all operators as individual tokens. 
* This means that while you can use value types such as bools and strings in expressions without issue, numbers have a bit of an unexpected footgun.
    - The expression `(-5)` returns the tokens [`-`, `5`], not `-5` as a user may expect.
    - This is compounded in more complex math expressions such as `(2 - -5)`, which returns the tokens [`2`, `-`, `-`, `5`].
    - Need to find an elegant solution to this so that users who _want_ all raw tokens for custom handling can get that, while users that expect [`2`, `-`, `-5`] get that.
