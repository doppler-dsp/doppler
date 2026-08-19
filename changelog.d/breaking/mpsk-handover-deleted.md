- **The MPSK acquisition/tracking handover is gone, and so is
    `ContinuousMpskReceiver`.** `acq_to_track`, the `tracking` property, the
    `<prefix>.tracking` telemetry probe, `MpskReceiver.configure_lock()` and
    the whole decision-directed arm are removed
    ([#877](https://github.com/doppler-dsp/doppler/issues/877)). One M-th-power
    NDA discriminator steers the LO from the first strobe to the last.

    **It was measured before it was deleted**, because `acq_to_track` had to be
    shown *not* inert rather than assumed so. The same record through
    `MpskReceiver` twice, at operating points where the meter is not saturated
    (SER 1.5e-2 to 7.1e-2, BPSK/QPSK/8PSK):

    | axis                          | result                                |
    | ----------------------------- | ------------------------------------- |
    | recovered symbols that differ | 19764–19928 of 19998 (~99%)           |
    | largest difference            | 0.30–0.48 (unit-radius constellation) |
    | SER over 10 engaged cells     | mean ratio **0.9999**, t = 0.28       |
    | cells where the handover won  | 6 of 10                               |

    So it perturbed essentially the whole sample path and moved the decisions
    by scatter with no sign. At the one operating point where it shipped
    enabled — 8PSK in `mpsk_receiver_ber.c`, on the ±π/8 decision-margin
    argument — the validator already carried its own number: turning it off
    costs **0.09 dB** (0.44 → 0.53 dB of loss) against a settling window it
    more than doubled, since the handover fired around symbol 8500.

    `ContinuousMpskReceiver` existed only to pin the handover off. With nothing
    left to pin it was a duplicate of `MpskReceiver`, which is now the
    continuous receiver under its own name. `configure_lock()` went for a
    second reason: it retuned only the handover's detector and never the lock
    indicator's, so it desynced the two detectors it appeared to configure.

- **`ber_settle_from()` loses its `handover` argument** (C and Python). Its
    only producer was `acq_to_track`, so it could now only ever be passed -1.
    `dp_ber_settle()` in the test harness loses the matching `tracking` array.

- **`MPSK_RX_LOOPS_STATE_VERSION` 6 → 7.** The blob drops the handover
    detector's `cnt`/`locked` pair and the `tracking` flag bit; `have_prev_idx`
    moves to bit 0. A v6 blob is rejected at the envelope, not reinterpreted.
    `MPSK_RX_HANDOVER_*` are renamed `MPSK_RX_LOCK_*`, since what they size is
    the surviving lock indicator.
