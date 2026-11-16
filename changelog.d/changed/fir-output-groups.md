- **`FIR` runs four vectors of outputs at a time, each its own running
    sum.** One sum was a chain of dependent adds as long as the filter. On
    an AVX2 + FMA build the real-tap rows now take 0.34-0.64 of the time
    and the complex-tap rows 0.23-0.31, the latter because complex taps
    ran scalar everywhere but AVX-512 and now have a vector kernel on AVX2,
    SSE2 and NEON. On a portable x86-64 build (the SSE2 tier of jm
    0.133.0) the real-tap rows take 0.34-0.53 of v0.84.0's time and the
    complex-tap rows 0.48-0.75. Outputs move in the last place under
    `-ffast-math`; a stream still gives the same bits however the caller
    splits it.
