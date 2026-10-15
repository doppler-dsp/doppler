- **`Synth.prod(x)` multiplies a block by the synth's own output.** `x` must
    be a 1-D C-contiguous complex64 block (a strided block or another dtype
    is refused); `doppler.wfm.rotate`, its swept-carrier special case, takes
    any 1-D complex64 view. The synth advances exactly as `steps(n)` does,
    and the result matches `x * synth.steps(len(x))` to float rounding, not
    bit for bit. C kernel: `dp_wfm_synth_prod (state, in, n, out)`.
