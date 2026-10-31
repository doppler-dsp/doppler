- **[Corr2D hypothesis bank: operation order](docs/design/corr2d-hypotheses.md)**,
    the pre-design study for #2683 and #2670, with a prototype and a C
    timing harness (`validate_corr2d_hypothesis_order`) whose results the
    page renders, from 32-sample codes to 1023. A bank shares the row FFT,
    pays the row inverses per carrier and the slow-time FFT per map: 1.5-6.5x
    faster than running every map through `DelayDoppler`. It mixes per
    fraction on an $L = 4$ grid (0.22 dB worst loss), and runs a short frame
    on one thread, where a pool is 1.5-8x slower.
