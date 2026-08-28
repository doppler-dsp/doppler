- **just-makeit pinned 0.69.2 → 0.70.0.** A struct field documented in a
    block *above* its declaration now derives its property docstring on both
    faces (jm gh-1167) — previously only the trailing `int span; /**< … */`
    form was read, so a `field = true` property whose prose already lived in
    the sacred header had to restate it in a manifest `doc` and maintain it
    twice. That was the third finding behind jm#1164, and it is doppler
    RFC #568's third open ask. Also lands `error_on_empty` (gh-1159), so a
    `variable_output` kernel that writes nothing can refuse instead of
    returning an empty array a caller silently accepts.
