- **Behavior change: `Ofdm.check()` raises instead of returning a negative.**
    It used to return `0` for a usable layout and a bare negative for an
    unusable one; it now raises `ValueError` naming the condition (no run, a
    window longer than an OFDM symbol, two fixed entries on one subcarrier)
    and returns `None` when the layout is usable, so a caller that tested
    `check() < 0` must catch the exception. C has `dp_ofdm_check_why()`;
    `dp_ofdm_check()` is unchanged. A C example, `ofdm_demo.c`, runs the
    lifecycle and checks itself, `modulate` has Python benchmark rows at the
    C rows' sizes, and the guide states the output's dtype, ownership and
    scale.
