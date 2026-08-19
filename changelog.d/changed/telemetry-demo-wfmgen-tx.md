- **The telemetry capture example's transmitter is `wfmgen`, not numpy.**
    `mpsk_telemetry_capture_demo.py` shaped its own rectangular pulse
    (`np.repeat`), applied its own carrier offset, and computed its own noise
    level from `sqrt(8 / (2 * 10**(20/10)))` — an Es/N0 convention written out
    by hand, where the `8` is `sps` and the `2` is the complex-noise factor and
    neither is named. It is now one `Composer([Segment(...)])`, the same path
    the CLI and a JSON record drive, with `snr_mode="esno"` stating the level
    once. numpy stays for analysis of the captured series.

    The point is not tidiness: this example exists to show what a receiver's
    telemetry can prove, and a demo that re-derives the transmitter cannot
    catch a transmitter bug — it would agree with itself while both halves
    drifted from the shipped generator.

    `scripts/check_stimulus_sources.py` is the gate for this class and passed
    on the old code, because its `pulse` marker matches a defined `rrc`/`rc`
    function and its `level` marker matches peak-fraction normalisation.
    Widening it is [gh-871](https://github.com/doppler-dsp/doppler/issues/871):
    the hand-computed sigma shape appears in 14 files, so it wants a ratcheted
    marker rather than a drive-by.

    The docstring also claimed the attach registers "all 13 probes" while the
    real figure is 16. It now names the probe families and points at the
    assert that pins the set (`set(series) == set(tlm.probe_names)`), a count
    nobody read back being exactly what went stale. Among the 16 are
    `rx.sync.lock` and `rx.sync.locked` — the timing loop's Gardner
    eye-opening ratio and its de-chattered flag, which no C accessor exposes.
