- **`FrameDesc.add_field` takes the spelling the constructor does.**
    `kind="pn"` and `lfsr="galois"` instead of the bare indices `1` and `0`,
    from the same `[[enum]]` the C enum backs — so a caller stops re-declaring
    the mapping by hand. The docstrings blamed
    [jm#1021](https://github.com/just-buildit/just-makeit/issues/1021) for the
    ints; that shipped in jm 0.64.0 and the note had been stale for eleven
    releases. `add_stage`'s `kind` stays an int, deliberately — see
    [#1223](https://github.com/doppler-dsp/doppler/issues/1223).
