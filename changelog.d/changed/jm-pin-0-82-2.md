- **just-makeit pin 0.82.1 → 0.82.2.** jm's generated element-contract test
    no longer ends in stray blank lines, so the pre-commit whitespace fixers
    stop rewriting a jm-owned file; and an edit to a declared feature after
    its binding was first rendered is now reported instead of silently
    ignored ([just-makeit#1432](https://github.com/just-buildit/just-makeit/issues/1432),
    doppler-driven).
