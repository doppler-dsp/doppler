- **Two more `mpsk_receiver_core.h` claims get C tests, both sabotage-proven**
    ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    **§14 — `bn_carrier` is normalised to the SYMBOL rate.** The `@warning`'s own
    headline: *"at the old default `sps = 8` the same number is now an 8x wider
    loop"*. Nothing measured it, so a regression to input-rate normalisation
    would have read correct at `sps = 8` — the rate every other test in the file
    uses — and been wrong everywhere else. Measured as settling time **in
    symbols** at one `bn` across a 4x span of `sps`, with the offset held
    constant in symbol-rate units: 320 symbols at `sps` 8, 16 and 32. An
    input-rate `bn` would scale that by 4, which is what makes the invariance the
    discriminator. Read off `get_norm_freq` rather than the lock detector, whose
    own EMA and verify counts made an earlier attempt useless (955 symbols at
    `sps = 4` against 51 at 16). Sabotage: scaling `bn_carrier` by `lo_sps` takes
    it red.

    **§15 — never pair `m_out = 2` with `MPSK_RX_PULSE_IANDD`.** The header says
    *never* and construction permits it anyway. Pinned as the **degeneracy**
    rather than the failure rate — *"fails about half the time"* is a
    distribution over seeds, not an assertion — and the mechanism is
    deterministic: **+11 dB** of EVM excess against `m_out = 8`'s +0.5. The test
    also asserts that `lock` stays above the declare threshold at both
    geometries, because it does: a caller who pairs 2 with I&D and watches
    `lock` sees nothing wrong, which is why the header's "never" could not be
    left to runtime.
