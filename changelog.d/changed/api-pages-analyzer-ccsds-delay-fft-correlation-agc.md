- **`doppler.analyzer`, `doppler.ccsds`, `doppler.delay`, `doppler.agc`,
    `doppler.correlation` and the FFT engines are documented by the
    generated Python reference.** Each package's overview, its flavor
    table for `Correlator`, and the worked examples (the CCSDS sync-marker
    search, the delay line's FIR loop, the AGC settling query, the inverse
    FFT) now open its reference page and run as tests; the FFT's
    single-precision path (PFFFT for 16-aligned, 5-smooth lengths) is
    stated in its header. The six hand-written API pages are gone (#3000).
