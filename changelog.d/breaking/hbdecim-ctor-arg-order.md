- **`HalfbandDecimator_create()` takes `(h, h_len)`, not `(num_taps, h)`** —
    the array first, matching its own manifest and every other array
    constructor in the tree. **C callers must swap the two arguments.** The
    types differ, so a call that does not swap fails to compile rather than
    misbehaving; the Python face is unchanged.

    `objects/HalfbandDecimator.toml` declared **one** constructor parameter
    while the C took **two**, in the opposite order. Nothing broke today,
    because jm never rewrites that prototype — but the manifest is what
    `jm regenerate` and every future reconciliation read, so the divergence
    was a trap for whoever regenerated the object next, and it hid the real
    signature from the generated stub.

    The issue proposed declaring both parameters instead, as the smaller
    change. Rendered, that turns out worse: jm sorts the array first and
    makes the count optional, giving
    `__init__(self, h, num_taps: int = ...)` — a redundant Python argument
    duplicating `len(h)`, and the C order still not what the manifest says.
    So the fix converges the C on jm's shape, which is also what
    `fir`, `corr`, `corr2d`, `detector` and `detector2d` already do; the
    `hbdecim` family were the only three `(len, ptr)` constructors in the
    tree, and the one-line adapter now does the swap.

    Not gated here, deliberately: verifying it needs jm's manifest→C type
    renderer, and reimplementing that downstream is the duplication jm
    exists to remove. `jm status --check` cannot see it — every file jm owns
    was self-consistent, and the only disagreeing file was the sacred
    `_core.c`. Filed as just-makeit#1076, which asks jm to verify the
    `_core.h` declaration it already injects.
