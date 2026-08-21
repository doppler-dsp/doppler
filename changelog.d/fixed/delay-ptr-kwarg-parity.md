- **`DelayCf64.ptr()` accepts the keyword its own type stub publishes, and
    a bare `ptr()` no longer promises one sample.** The stub said
    `ptr(count=1)`; the binding accepted `n=` and returned the whole window.
    Both halves were wrong, and in opposite directions — a caller following
    the stub got `TypeError: 'count' is an invalid keyword argument`, while
    one who found `n=` was told the default was 1 when it was `num_taps`.

    The keyword is now `count`, which every published face — the stub, the
    runtime docstring — had said all along; nothing in the tree passed it by
    keyword, so no caller moves. The default is now **declared** rather than
    hand-restored after each regeneration: `count_default` in
    `objects/delay.toml` is a C expression (just-makeit gh-1051), which is
    what lets an instance-derived default live in the manifest at all. The
    hand-patch it replaces is gone, so it cannot drift again.

- **A gate for the whole class: `make kwarg-parity-check`.** It reads the
    `_kwlist` out of every `native/src/*/*_ext_*.c` and compares it to the
    `def` its `.pyi` publishes — 169 methods, discovered, so a new object is
    covered the moment its fragment exists.

    `jm status --check` structurally cannot see this: the kwlist sits in a
    wrapper body, which jm's own output calls out as *"yours … not counted
    as drift"*. Measured, not assumed — renaming the kwarg back to `n`
    leaves `make drift-check` reporting exactly what it reported before, and
    passing.

    It found two more on arrival, both the reverse pairing (the stub
    *under*-publishes a hand-written `out=`), and carries them as a
    shrink-only ratchet: an entry that stops mismatching also fails, so the
    list cannot rot in either direction. doppler#922 tracks emptying it;
    just-makeit#1074 is the upstream gap underneath both findings.
