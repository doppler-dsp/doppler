- **`Ofdm.check()` says why.** It raises `ValueError` naming the condition (no
    run, a window longer than an OFDM symbol, two fixed entries on one
    subcarrier) instead of returning a bare negative; C has
    `dp_ofdm_check_why()`. A C example, `ofdm_demo.c`, runs the whole
    lifecycle and checks itself, a `modulate` benchmark row exists in Python
    for each of the C rows, and the guide states the output's dtype,
    ownership and scale.
