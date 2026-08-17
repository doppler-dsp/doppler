- **What `mf_in` costs is excess noise bandwidth, not processing gain.** A
    Nyquist-sampled band-limited signal loses nothing by being sampled fast.
    Measured at the node with the AGC off so the path is linear, `mf_in` sits
    **6.01 dB** below Es/N0 at `bank_sps = 4` while the terminal node sits
    1.7 dB below it — and `10·log10(4) = 6.02 dB`, *identical at 6.79, 12 and
    20 dB Es/N0*, which is the signature of a pure bandwidth ratio rather than
    an SNR-dependent effect. DEC band-limits to **its own** Nyquist,
    `±bank_sps·Rs/2`, while the signal occupies ~`±Rs`, and the terminal filter
    — the first thing in the cascade matched to the signal — is downstream of
    this tap.

    So the cost is **bounded by the plan** (`bank_sps` is a planner outcome:
    still 8 at `sps = 64`, so 9.0 dB there, not 18) — and it is the tap's
    **stated price rather than a defect**. Band-limiting the node (an arm
    filter, or the 2-sps decimation `docs/design/mpsk.md` §3.3 considers)
    would recover most of it and is **declined**: both cost serialized state
    on every object carrying the tap, and `strobe` already reads the node
    matched to the signal for free — and measures better on this flavor's own
    waveform. So `mf_in`'s trade is stated, not repaired: `bank_sps/(2M)` of
    pull-in range for `10·log10(bank_sps)` dB of lock sensitivity. The loop
    acquires at every operating point measured; what degrades is the
    M-th-power lock statistic, which is an SNR measure.
