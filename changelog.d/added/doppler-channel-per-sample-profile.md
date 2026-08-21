- **`DopplerChannel` takes a Doppler PROFILE — one value per waveform
    sample.** The create-time `(doppler_ppm, doppler_rate_ppm_s)` pair is a
    straight line, and a satellite pass is not one. `execute_profile(x, ppm)`
    supplies the Doppler as an array parallel to the input instead, so a
    measured or modelled pass drives the channel directly.

    That length contract is not a convenience — it is the contract
    `resamp_execute_ctrl()` underneath **already has** (`ctrl` parallel to
    `in`), so a profile is handed to the resampler rather than reduced to fit
    it. The shape matches `Resampler.execute_ctrl` for the same reason.

    **The profile is absolute**, and the create-time scalar cancels exactly
    rather than by convention: the resampler's rate is `base + ctrl`, and the
    kernel fills `ctrl = ratio(ppm[i]) - base` with the same `base` it was
    built with. **The length is enforced, not documented** — jm gives each
    array its own length, and a mismatched profile is a rejected call instead
    of a silent read of whichever array ran out first.

    The one thing a profile cannot have is the closed form. `phase(t) =   fc * excess(t)` has no counterpart for an arbitrary sequence, so the
    excess-delay integral is accumulated as the stream advances. Two
    consequences, both deliberate: it is summed in *seconds* rather than
    cycles, which is what keeps it affordable (excess stays ~1e-2 s over a
    long capture while the phase it implies is ~5e7 cycles, so the running
    sum never carries the large magnitude); and it is running state, so it
    joins the two sample clocks in the serialized blob —
    `DOPPLER_CHANNEL_STATE_VERSION` is now 2. Without it a checkpoint would
    resume a curved pass at zero excess and step the carrier at the seam, in
    the one mode with no closed form to recover it from.

    Order of accumulation is load-bearing and was caught by test rather than
    review: reading `excess` at the sample and advancing afterwards makes it
    a left sum equal to the closed form's `d*t`. Advancing first gives every
    sample the *next* sample's phase — one increment is 2.9 degrees at 20 ppm
    on a 2.5 GHz carrier at 6.138 Msps, 0.05 of amplitude, which is exactly
    what the flat-profile equivalence test failed on.

    Proven by five C sections and six Python tests: a flat profile reproduces
    the scalar route, a linear one reproduces the ramp, a sign change
    mid-record measures ±50 kHz where no `(d0, d_dot)` could put it, a
    mid-stream split resumes bit-exact, and invalid/NULL/mismatched-length
    calls write nothing rather than a valid prefix. Four mutation sabotages
    were run against those assertions — dropping the accumulator from the
    blob, skipping validation, breaking absoluteness, ignoring the profile in
    `offset_hz` — and all four went red.

- **`doppler_channel_demo` gains a fourth panel: a real LEO pass.** Doppler
    from circular-orbit geometry for a 550 km overhead pass — the law of
    cosines for slant range, differentiated — which comes out an S-curve of
    +23.3 to −23.3 ppm (±58.2 kHz at 2.5 GHz) that departs from its own best
    straight line by **21% of its range**. That residual is the panel's
    assertion, and it is the argument for the array form in one number.

    It also surfaces something the scalar form cannot show: over a whole pass
    the **net dilation is +0.000 ppm**. The record is compressed while the
    satellite closes and stretched by the same amount while it opens, because
    an overhead pass is antisymmetric about closest approach — so the
    instantaneous rate error reaches 23.3 ppm and the totals cancel, which is
    why a receiver must track a pass rather than fit one clock offset to the
    capture.

- **The `doppler_channel` C benchmark now warms the process before timing any
    configuration, and its published ratios changed as a result.** The
    per-config warm-up was not enough on its own: the first configuration
    still absorbed the CPU's frequency ramp out of a cold process, and MIN
    over rounds cannot remove it because every round in a cold process is
    equally cold. Charged to whichever config ran first, that produced ratios
    *below* 1.0 against the first row — `ramp/static` read **0.70x**, i.e. "a
    drifting offset is 30% cheaper than a fixed one", beside prose asserting
    they cost the same. It now reads **1.00x**, which is what the prose said
    all along. An instance of doppler#896.

    With the measurement trustworthy, the new `execute[profile]` row reads
    **0.96x** against static, reproducibly: the array form is marginally
    *cheaper*. Both paths divide once per sample to turn a Doppler into a
    rate and both feed the same resampler and complex multiply; what the
    closed form adds is deriving `t` per sample on both clocks, where the
    array reads `ppm[i]` and accumulates. So a measured LEO profile is not a
    stimulus a harness needs to ration — which is the whole question a caller
    has about the array form.
