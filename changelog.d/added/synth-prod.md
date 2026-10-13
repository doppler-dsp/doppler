- **`Synth.prod(x)` multiplies a block by the synth's own output, and
    `doppler.wfm.rotate` is its swept-carrier special case.** A tone makes it
    a rotation, a chirp a swept rotation, noise a noise multiplier, PN/BPSK a
    modulation; the synth advances exactly as `steps(n)` does, so split calls
    leave the same state (the product agrees to rounding, not the bit).
    `rotate(x, freq, rate, fs, center)` sweeps about a stated center; the C
    kernel is `dp_wfm_synth_prod (state, in, n, out)`, the twin helper
    `native/tests/dp_swept_test.h`.
