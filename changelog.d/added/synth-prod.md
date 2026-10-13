- **`wfm_synth` multiplies a block by its own output: `dp_wfm_synth_prod` /
    `_SynthEngine.prod`, and `doppler.wfm.rotate` as the swept-carrier
    special case.** A tone makes it a rotation, a chirp a swept rotation,
    noise a noise multiplier, PN/BPSK a modulation; the synth advances exactly
    as `steps(n)` does, so split calls equal one call. `rotate(x, freq, rate,   fs, center)` applies a chirp about a stated center; the C twin is
    `native/tests/dp_swept_test.h`.
