- **`MpskReceiver`'s tests seeded the carrier loop either ON the answer or
    past its pull-in cliff, so almost none of them measured acquisition**
    ([#843](https://github.com/doppler-dsp/doppler/issues/843)). Every seeding
    site is now stated in the loop's own units — cycles per **symbol**, the
    same normalization as `bn_carrier` and `bn_timing` — and held at or under
    the acquisition bound of `bn_carrier / m`.

    The `m` is the part that was missing everywhere.
    `_mpsk_rx_harness.freq_offset_inside_bw` returned `frac * bn / sps`,
    contradicting `docs/design/mpsk-refactor.md` §4.4 on two counts: the NDA
    discriminator is an M-th power, so the bound carries a divide by `m`, and
    the bound has no `sps` in it at all. Without the `m` one call read
    identically at every order while asking a 4× harder question at 8PSK than
    at BPSK, putting 8PSK exactly on the measured limit with no margin. The
    same expression was written out by hand in the C BER certification
    (`mpsk_receiver_ber.c`, `mpsk_receiver_real_ber.c`), whose sweep runs all
    three orders.

    Conversion to cycles per **sample** now happens once, at the constructor
    boundary that needs it, which is what `native/tests/dp_rx_mpsk.h` already
    warned about: *"Mixing them is an sps-sized error, and at sps=8 it asked
    the loop for 8x its design envelope."*

    The opposite defect sat in `test_mpsk_receiver.py` and
    `test_mpsk_receiver_core.c` §2, which passed `init_norm_freq` equal to the
    stimulus's own offset — so the loop started on the answer and never left
    its initial state. **A receiver whose carrier discriminator was wired to
    nothing passed all of those.** They now start one bound below truth, and
    the change is sabotage-proven: moving the seed to ten times the bound
    fails six of them, where seeded on truth they passed at any offset
    whatsoever.

    Bounds measured 2026-08-17 rather than asserted (6 seeds per point;
    BPSK/QPSK/8PSK at sps 8, bn 0.01, 20 dB, 4000 symbols): the carrier loop
    acquires reliably out to 4× its bound and collapses by 6×, so the rule
    keeps a 4× margin; the timing loop reaches 1.6× and collapses over
    1.8–2.0×, and `clock_offset_inside_bw` was confirmed correct as written —
    no `m` belongs in it, because the timing discriminator is not an M-th
    power.

    `scripts/check_stimulus_sources.py` gains a fourth signature, so a bare
    cycles-per-sample offset literal in the test, validation or example trees
    fails the gate the way a private pulse, level or EVM already does.
