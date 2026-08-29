- **just-makeit pinned 0.69.2 → 0.70.1.** A struct field documented in a block
    *above* its declaration now derives its property docstring on both faces
    (jm gh-1167) — previously only the trailing `int span; /**< … */` form was
    read, so prose already in the sacred header had to be restated in a
    manifest `doc` and maintained twice. 0.70.1 also carries jm#1177, a
    doppler-filed regression in 0.70.0 where a view stopped inheriting its
    parent's `create()` `@param` prose: params inherit, the summary does not.
    Also lands `error_on_empty` (gh-1159), so a `variable_output` kernel that
    writes nothing can refuse instead of returning an empty array.
