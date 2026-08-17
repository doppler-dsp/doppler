- **`MpskReceiver`'s `resolves` verdict is derived from the experiment's design,
    not from its outcome.** It keyed on the *measured* error count clearing
    `MIN_ERRORS`, which was wrong twice over. Statistically: whether an
    experiment can resolve an effect is fixed by the bound and the record length
    before any data arrives, so reading it off the observed count judges a
    receiver measurable *because* it did worse. Practically: the measured count
    is machine-dependent, so 8PSK at 17 dB gave 20 errors on one toolchain and 6
    on another, straddling the floor — `resolves` read `yes` on one machine and
    `no` on the other, and **the set of asserted limits changed with it**. A cell
    dropped out of the certified envelope depending on which compiler built it.

    Now `theory * scored symbols >= MIN_ERRORS`, which is a closed form over
    `erfc` and bit-identical across those toolchains (measured). The measured
    `errors` column stays in §2.1 — it is what tells a reader whether the
    design's expectation was borne out. Found by the structural comparison in
    [#820](https://github.com/doppler-dsp/doppler/issues/820), which masked every
    number and left exactly this verdict flip visible.
