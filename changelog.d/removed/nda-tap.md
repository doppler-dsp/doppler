- **`nda_tap` is gone; the carrier discriminator reads the on-time strobe.**
    The knob offered three nodes and the other two are deleted with it, along
    with `MPSK_RX_NDA_TAP_*`, the `mf_out`/`mf_in` code paths, and the
    `rx_nda_tap.c` harness that ranked them
    ([#832](https://github.com/doppler-dsp/doppler/issues/832)).

    Measured on the receiver's own waveform — NRZ, modulation off then dense,
    under a coupled Doppler ramp — the strobe won on every axis: lock **+0.860**
    at the data onset against `mf_out`'s +0.478 and `mf_in`'s +0.417. Its
    timing dependency costs nothing exactly where timing is impossible, because
    an unmodulated carrier is sampling-phase invariant.

    The pull-in range the taps traded for is not lost, it is derived: an
    M-th-power detector updating at `F` sees `|Δf| < F/(2M)`, and the strobe
    fixes `F = Rs`. A caller needing more states the requirement and gets a
    loop that meets it or a refusal (`docs/design/mpsk.md` §8.2).
