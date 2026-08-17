- **The receiver battery is a complete suite, and it runs on two receivers.**
    Four operating points added (`qpsk`, `psk8`, `irrational` at
    `sps = 17.33389`, `rate_odd` at `sps = 31.7`), and a second adapter for
    `ContinuousMpskReceiver` — which is `docs/design/rx-test.md` goal 6 cashed
    rather than asserted: the second receiver costs one function and reuses the
    other ten interface entries unchanged. Each M is read at **its own**
    SER=1e-3 Es/N0 (6.79 / 10.35 / 15.68 dB from `ber_esn0_db_for_ser`),
    because holding one Es/N0 across M compares constellations rather than
    receivers.

    What it found immediately, and none of it was visible to the Python
    harness it replaces:

    - **`nda_tap = mf_in` refuses on all 9 points** of the battery, under
        either pulse, on the BASE receiver — isolated by changing only that
        one argument. Diagnosed under Changed: the tap acquires everywhere,
        and it is its LOCK STATISTIC that falls under the detector's
        threshold, so the refusals are the settle gate reading a degraded
        indicator rather than a loop that never moved.
    - **Implementation loss grows with irrational oversampling**: 0.07 dB at
        `sps = 8`, **4.34 dB** at 17.33389, **7.41 dB** at 31.7 — a trend, not
        a cliff, and defensible because the harness's four gates passed.
    - `qpsk`/`psk8` **refuse** on frame geometry (the frame's bit count does
        not divide into whole symbols at M = 4/8). A refusal is a result: the
        frame set is BPSK-shaped and the M sweep needs a length that divides.
