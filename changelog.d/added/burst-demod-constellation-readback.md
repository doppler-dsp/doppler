- **`BurstDemod.symbols()` — the constellation the LLRs are the real part
    of.** The object built the derotated, unit-normalised complex symbols
    either way (the LLR projection and the noise estimate are both made from
    them) and then freed them unread. After derotation the real axis carries
    the signal and the imaginary axis carries noise alone, so a
    phase-coherence problem lands in Q and **nowhere else**: an untracked
    Doppler rate raises Q/I 41x while `est_snr_db`, `est_rate_hz` and the
    decoded bits are all unchanged. `est_n0` is now a read-back too, so the
    LLR scale can be undone rather than just documented.
    [#1087](https://github.com/doppler-dsp/doppler/issues/1087).
