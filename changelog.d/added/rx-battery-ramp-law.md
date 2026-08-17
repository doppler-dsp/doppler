- **The receiver battery measures the ramp law.** `dp_rx_result_t`'s
    `ramp_lag_rad`/`ramp_law_rad` had been declared since the instrument
    landed and filled by nothing. Filling them found two reasons the
    `doppler` point could not have measured anything, and both were in the
    measurement rather than the receiver.

    **The point was three and a half decades below the answer.** It carried
    0.02 ppm/s, which `2*pi*r/wn^2` puts at a settled lag of 2.2e-4 rad
    against a linear range of `pi/4` — so every number it produced was
    byte-identical to `anchor`'s, and a point that reproduces the reference
    is not measuring the thing it is named for. The rate is now sized from
    the answer it has to make observable: **9.2 ppm/s**, predicting ~0.1 rad,
    an eighth of the range, so the *law* is checked rather than its
    breakdown.

    **The estimator was reading its own noise.** The discriminator series was
    reduced to `fabs()` at capture, and at these Es/N0 the discriminator's
    noise dwarfs the lag — `|e|` has an RMS of 0.54 against a lag of 0.1, so
    a mean of it is nearly identical at every point in the battery. The
    series is now **signed** and the magnitude taken of the **mean**: the
    loop's own integrator forces that mean to the value which sustains the
    ramp, so the noise averages out of it. Measured **0.1006 rad against
    0.09989 predicted, +0.7%** — the agreement `rx_nda_tap.c` gets on a
    *noiseless* tail, here at 6.79 dB Es/N0.

    Gated at the 10% tolerance `rx_nda_tap.c` established, and only where a
    ramp exists: a type-2 loop nulls a frequency step regardless of gain, so
    an unimpaired point has a law of exactly zero and nothing to check.
    `wn` comes from the new `loop_filter_wn()`, and the damping it needs is
    **read back from the constructed receiver** through a new `zeta` entry on
    `dp_rx_iface_t` — the adapter passes `0` and asks the receiver to derive
    it, so restating the default would have been a copy of the number the
    receiver is free to change.

    Sabotage, both ways: scaling `freq_scale` by 2 reads +101.4% and fails,
    by 1.25 reads -19.4% and fails — **and the trio stays green through
    both** (SER 1.09e-3, EVM -7.35 dB, unmoved). A carrier loop running at
    twice its stated bandwidth is invisible to every other number the
    instrument produces.

    **What it does not cover, structurally.** gh-765 itself was `freq_scale`
    missing its `* upd` — the filter's output taken as radians per *update*
    rather than per symbol. Every battery point runs `nda_tap = 0` (STROBE),
    whose update rate is exactly 1, so that factor *is* 1 and removing it
    leaves this gate byte-identical and green. `rx_nda_tap.c` catches it, on
    the taps whose update rate is not 1 (`mf_out` 2.0, `mf_in` 1.5625) and
    never on `strobe`. A battery point at a non-unity tap is what would close
    it, and that is entangled with the open `nda_tap` question (#791).
