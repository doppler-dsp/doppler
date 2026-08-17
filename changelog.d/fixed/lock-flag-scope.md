- **The lock detector's DETECTION side is measured, and its scope is written
    down.** `MPSK_RX_LOCK_THRESH_DEFAULT` is derived as
    `sigma_H0 * eta(Pfa)` — a false-alarm threshold sized against the
    no-signal distribution alone — and the header's claim that the statistic
    "reads ~1.0 at lock" carried no Es/N0 with it. It does at the design
    point: **100 % duty at every named battery point**, each at its own
    SER = 1e-3 anchor. It does not below: 69 % at +1 dB, **24 % at 0 dB**, 0.2
    % at −3 dB — while the statistic stays positive throughout and a
    concatenated link over that same record delivers error-free frames. So
    the default is an **uncoded-link indicator**, and a caller running where
    FEC exists to put you must gate on frame sync or on `node_sync_score`
    instead. [#835](https://github.com/doppler-dsp/doppler/issues/835).

    Reported and not gated, deliberately: a `lock_duty >= 0.9` gate was
    written and removed after sabotage, because `dp_ber_settle` already
    requires the flag to hold 90 % over 200 symbols and both sabotages
    reddened the tally gate first. A gate that cannot fail independently is
    one nobody can trust the day it goes green.
