- **`MpskReceiver`'s tests seeded the carrier loop either ON the answer or
    past its pull-in cliff, so almost none of them measured acquisition**
    ([#843](https://github.com/doppler-dsp/doppler/issues/843)). Every seeding
    site is now stated in units of the loop's own acquisition bound — `u`, the
    offset over `bn_carrier / m` cycles per symbol — and held to `0 < u <= 1`.

    The `m` is the part that was missing everywhere.
    `_mpsk_rx_harness.freq_offset_inside_bw` returned `frac * bn / sps` with no
    `m` in it, contradicting `docs/design/mpsk-refactor.md` §4.4; because the
    NDA discriminator is an M-th power, that hides a factor of `m`, so one call
    reads identically at every order while asking a 4× harder question at 8PSK
    than at BPSK. Its `frac=0.5` default put 8PSK at `u = 4.0` — measured to be
    exactly the reliable limit, with no margin. The same expression was written
    out by hand in the C BER certification (`mpsk_receiver_ber.c`,
    `mpsk_receiver_real_ber.c`), whose sweep runs all three orders.

    The opposite defect sat in `test_mpsk_receiver.py` and
    `test_mpsk_receiver_core.c` §2, which passed `init_norm_freq` equal to the
    stimulus's own offset: `u = 0`, so the loop started on the answer and never
    left its initial state. **A receiver whose carrier discriminator was wired
    to nothing passed all of those.** They now start one bound below truth, and
    the change is sabotage-proven — moving the seed to `u = 10` fails six of
    them, where at `u = 0` they passed at any offset whatsoever.

    Bounds measured 2026-08-17 rather than asserted (6 seeds per point;
    BPSK/QPSK/8PSK at sps 8, bn 0.01, 20 dB, 4000 symbols): the carrier loop
    acquires reliably to `u = 4` and collapses by 6, so the rule keeps a 4×
    margin; the timing loop reaches `u = 1.6` and collapses over 1.8–2.0, and
    `clock_offset_inside_bw` was confirmed correct as written — no `m` belongs
    in it, because the timing discriminator is not an M-th power.

    `scripts/check_stimulus_sources.py` gains a fourth signature, so a bare
    cycles-per-sample offset literal in the test, validation or example trees
    fails the gate the way a private pulse, level or EVM already does.
