- **`Synth.prod(x)` multiplies a block by the synth's own output.** `x` must
    be a contiguous complex64 block (a view or other dtype is refused);
    `doppler.wfm.rotate`, its swept-carrier special case, takes any 1-D view.
    The synth advances exactly as `steps(n)` does; the product agrees to
    rounding, not the bit. C kernel: `dp_wfm_synth_prod (state, in, n, out)`.
