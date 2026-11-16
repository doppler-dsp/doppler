- **`FIR` runs four vectors of outputs at a time, each its own running
    sum.** One sum was a chain of dependent adds as long as the filter. On
    an AVX2 + FMA build the real-tap rows now take 0.34-0.64 of the time
    and the complex-tap rows 0.23-0.31, the latter because complex taps
    ran scalar everywhere but AVX-512 and now have a vector kernel on AVX2
    and NEON too. A portable x86-64 build has no vector tier yet and is
    unchanged. Outputs move in the last place under `-ffast-math`; a stream
    still gives the same bits however the caller splits it.
