- **just-makeit pin 0.73.0 → 0.73.1**, and ten `tlm` init-params adopt
    gh-1224's `object = "dp_tlm"` in place of a capsule name spelled at both
    ends with nothing checking they agree. 0.73.1 carries
    [just-makeit#1234](https://github.com/just-buildit/just-makeit/issues/1234),
    filed from here, plus gh-1229 — 20 `[module.X]` keys the validator accepted
    and the writer silently dropped on the next mutating command, `capsule`
    among them.
