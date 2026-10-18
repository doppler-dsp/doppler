- **just-makeit pin 0.106.0 → 0.108.0: the stubs mark positional-only
    parameters with `/`.** A type checker now refuses the keyword calls
    those bindings always refused at runtime (`acc.step(x=1.0)`); nothing
    at runtime changes. The rest is generated binding glue. The C tests
    spell `complex` as `_Complex` (same type), and the mutator-state gate
    reads `/` parameters (#2428), so those 40 methods are still probed.
