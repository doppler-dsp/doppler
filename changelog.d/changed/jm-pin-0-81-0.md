- **just-makeit pin 0.79.1 → 0.81.0.** Pure tooling: `jm apply` changes no
    generated file. A borrowed view now honours `nogil` and `none_on_empty`
    and says *why* it returned NULL (`status_fn` + `status_errors`, jm
    gh-1418, doppler-driven) — enough to generate the ring's binding to
    66 of its 72 tests; the last three gaps are
    [just-makeit#1426](https://github.com/just-buildit/just-makeit/issues/1426).
