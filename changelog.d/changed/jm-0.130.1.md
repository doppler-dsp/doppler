- **just-makeit pin 0.126.1 → 0.130.1.** The re-vendored `jm_simd.h` has
    an SSE2 tier: a portable x86-64 build runs the FIR, AGC, accumulator
    and ADC kernels four float lanes wide where it ran one, and a result
    there can differ in the last place (SSE2 has no fused multiply-add).
    Generated code is otherwise unchanged: comments, enum-table
    docstrings, and two private names out of `doppler.wfm.__all__`.
