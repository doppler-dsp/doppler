- **just-makeit pin 0.134.1 → 0.135.0.** A defaulted parameter's docstring now names its default (`default None` where it read `default ...`), a bit-pattern CLI flag reads as `0x` hex, and a bytes field of a generated kind is copied and freed by a declared rule rather than per-field code
    ([just-makeit#2701](https://github.com/just-buildit/just-makeit/issues/2701),
    [just-makeit#2703](https://github.com/just-buildit/just-makeit/issues/2703),
    [just-makeit#2706](https://github.com/just-buildit/just-makeit/issues/2706)).
    The re-render changes five extension files and four stubs; no core changes.
