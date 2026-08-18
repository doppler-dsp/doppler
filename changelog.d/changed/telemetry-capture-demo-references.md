- **The telemetry capture demo reads every panel against the number it is
    supposed to hit.** Each trace now carries its reference: decision
    thresholds in red, read off the receiver rather than retyped, and the
    actual quantity an estimator is estimating in green. `sym.i` and `sym.q`
    share one axis, because what matters is their relative size — I settles
    into two ±1 bands while Q collapses onto zero, and separate autoscaled
    panels render a Q of pure noise exactly like a Q carrying signal. The
    example asserts it (mean|I| ≈ 17× mean|Q|) rather than leaving it to
    the eye.

    The demo also stops passing `acq_to_track=1`: **there is no handover.**
    One NDA discriminator steers the LO from the first output to the last,
    which is Mode 1 in `docs/design/mpsk.md`, and the demo had been running
    the superseded design against a view whose own manifest pins that gating
    at 0. The parameter remains on the shipped constructor — measured, it
    still changes 456 of 3998 symbols — so retiring it belongs to
    [#831](https://github.com/doppler-dsp/doppler/issues/831).
