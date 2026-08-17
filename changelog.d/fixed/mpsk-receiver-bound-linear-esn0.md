- **Every `SER theory` figure in the `MpskReceiver` validation report was the
    bound at the wrong Es/N0.** `ber_theory_ser` takes **linear** Es/N0 — its
    header says so in capitals — and that validator passed dB, alone in the
    tree (`ber_esn0_db_for_ser.c`, `ber_awgn_demo.py` and
    `test_mpsk_receiver_performance.py` all convert).

    The consequence was not an offset but a **fold**: `db → 10·log10(db)` is
    compressive, so an 8/12/16 dB sweep was scored against the bound at
    9.0/10.8/12.0 dB and the sign of the error reversed across the sweep. It
    read as a receiver falling behind the bound at low Es/N0 and catching up
    at high — which is what an implementation loss is supposed to look like,
    and is why it survived review. The tell was 8PSK reading **20 dB better
    than the bound**; nothing beats a matched filter, so a negative
    implementation loss is always a defect in the measurement.

    Corrected, **BPSK's implementation loss is +0.47 to +0.67 dB and QPSK's
    +0.33 to +0.39 dB**, where the old table implied ~1.3 dB at BPSK and a
    10× rate deficit at QPSK. Every bound now goes through one
    `_bound(m, esn0_db)` helper, so there is a single conversion site rather
    than three that agreed by luck. Recorded as the report's F8.
