- **One `mpsk.md`.** The M-PSK design lived on three pages —
    `mpsk.md`, `mpsk-refactor.md` and `mpsk-soft.md` — and two of them
    specified the *same constructor differently*. They are now one page:
    the refactor's API surface merged into §8 with the disagreements resolved,
    its collapse record kept as §12, and the soft-decision design folded under
    §9 as §9.7, rewritten to describe what shipped rather than what was
    proposed.

    Where the two constructors disagreed, §8.2 is the answer: `nda_tap` and
    `acq_to_track` are **gone** rather than derived, and `differential` moves
    to `bits()`. §8.2 also now states the whole target — **exactly two required
    arguments**, `sample_rate_hz` and `symbol_rate_hz`, with `m` defaulting to
    2 — and the three-constraint rule that derives `bn_carrier`.
