- **just-makeit pin 0.103.0 → 0.104.0.** An integer outside its C type's range
    now raises `OverflowError` instead of wrapping silently into C (jm
    gh-2144): `int8_t` given `300` used to arrive as `44`. It holds for every
    regenerated constructor, setter, method and module function;
    `uint64_t` and `size_t` still wrap (jm gh-2220). The bump also brings
    `out=` and `<m>_max_out` for a `variable_output` method with an input and
    params (gh-2028) and an exact 64-bit composer JSON face (gh-2139). The
    regenerated sacred `_ext` fragments are the only other change.
