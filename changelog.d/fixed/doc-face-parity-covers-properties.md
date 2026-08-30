- **Doc face parity covers properties now, and nine were already stale.** The
    check compared methods only, so a property's `.pyi` could move while its
    runtime `__doc__` sat unchanged — `help()` and a type checker telling a
    reader different things, with every gate green. Found by walking into it:
    `refine_span`'s correction reached the stub and not the fragment, and the
    gate reported OK across the divergence. Properties are compared whole
    (prose has no numpy sections to compare) with whitespace normalised, so
    only the words count.
    [#1090](https://github.com/doppler-dsp/doppler/issues/1090).
