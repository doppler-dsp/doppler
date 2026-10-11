**just-makeit pin 0.100.1 → 0.103.0.** Project schema 8 → 9
(`[app]` is now `[[app]]`). It brings `out_cols`, so a `variable_output`
method can return a `(rows, nfft)` matrix (jm gh-2115), and a property
setter that can refuse a value (gh-2182). The regenerated stubs and
sacred `_ext` fragments are the only other change.
