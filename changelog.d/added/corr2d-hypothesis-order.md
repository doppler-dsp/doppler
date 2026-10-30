- **[Corr2D hypothesis bank: operation order](docs/design/corr2d-hypotheses.md)**,
    the pre-design study for #2683 and #2670, with a prototype and a C
    timing harness (`validate_corr2d_hypothesis_order`) whose results the
    page renders. A bank shares the row FFT, pays the row inverses per
    carrier and the slow-time FFT per map. That ordering is 1.6x faster than
    running every map through `DelayDoppler` at $n_x = 512$, and 5x faster at
    $n_x = 1023$. The two exact ways to apply a fractional carrier offset
    time within 10% of each other.
