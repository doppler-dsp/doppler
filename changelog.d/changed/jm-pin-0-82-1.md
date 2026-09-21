- **just-makeit pin 0.82.0 → 0.82.1.** Pure tooling: `jm apply` changes no
    generated file. A `header_only` component can now declare a dependency
    (its link line said `PUBLIC` on an `INTERFACE` library and did not
    configure), which is what lets the ring's generated binding join the
    process-wide interrupt; and jm's generated element-contract test can run
    ([just-makeit#1432](https://github.com/just-buildit/just-makeit/issues/1432),
    doppler-driven).
