- **`detection` is certified** — the sizing helpers `Acquisition`, `LockDet`,
    `Dll`, `BurstDespreader` and `BerMeter` all share, so a defect in these 19
    functions moves every detector at once. 20 limits on every push. The
    header's headline number — a chi-square gate on an estimated noise
    reference realizing 41x the priced `pfa` at n = 16 — executed nowhere; it
    is now re-derived in C, reproduced by Monte-Carlo (41.1x), and restated as
    the number a caller can budget: **3.27 dB at n = 16, 0.97 dB at n = 100**.
    One finding is open and not this object's to fix
    ([#997](https://github.com/doppler-dsp/doppler/issues/997)). Evidence:
    `src/doppler/detection/tests/validation/detection/results.md`.

- **`docs/design/detection.md`** — the page this module never had. Five
    statistical families ship under one `det_` prefix and are not
    interchangeable, and reaching for the wrong one does not fail loudly: at
    `pfa = 5e-6`, `det_threshold` returns 4.9409 and `det_q_inv` returns
    4.4172 — two plausible numbers near 5, only one of which is a sigma count.

- **`models` characterization subject** — the Monte-Carlo the tree cited but
    never ran (`acq_core.c`: *"Validated against Monte-Carlo to \<1%"*). Every
    family's H0 rate across four decades of `pfa` and both Pd curves against
    measured frequency, 2 million draws per cell, every cell within 3 sigma.
