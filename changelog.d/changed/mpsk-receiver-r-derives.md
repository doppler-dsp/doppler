- **`MpskReceiverR` derives the five parameters its complex twin derives.**
    `m_out`, `zeta`, `lock_thresh`, `num_phases` and `bn_agc_ratio` now
    default to `0` — *derive it* — instead of carrying pinned values.
    `MpskReceiver` and `MpskReceiverR` are one object with two constructors,
    and both now read back the same numbers:

    ```pycon
    >>> MpskReceiverR(m=4, sps=32.0).num_phases
    64
    ```

    which was `1024`. Three of the five were not merely redundant:
    `num_phases = 1024` was the legacy bank against the **measured**
    saturation point of 64, `lock_thresh = 0.5` a round number against the
    derived `sigma_H0 * eta(Pfa)` = 0.4999, and `zeta = 0.707` a typed-out
    constant against `1/sqrt(2)`. `m_out = 8` happened to equal the
    derivation at the default `sps = 32` and stopped doing so anywhere else.

    The real face never adopted `docs/design/mpsk.md` §8.1 because the
    collapse that created it carried the defaults across **unchanged** — a
    refactor and a retune in one commit is a diff nobody can bisect. This is
    the retune, on its own, with the evidence.

    **Nothing measurable moved.** `validate_mpsk_receiver_real_ber` reports
    implementation loss against the coherent bound at each M's own SER=1e-3
    anchor, and every figure is identical before and after — 0.54 dB at
    M=2, 0.51 dB at M=4, 0.92 dB at M=8. So the saturation measured on the
    complex face holds behind the R2C halfband too, and the 16x bank was
    paying about **40 kB per instance** (measured over 100 instances) for
    resolution the receiver cannot use.

    Pass a value to pin one, exactly as before; only the defaults moved.
