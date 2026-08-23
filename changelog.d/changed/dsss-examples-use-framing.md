- **The DSSS examples describe their burst to wfmgen instead of assembling
    it.** `wfmgen` has owned `[preamble × reps | sync | payload | CRC-16]`
    as a `--type dsss` waveform for a while; three examples still built one
    by hand, so the framing rules lived in four places.

    `dsss_realtime_file_demod.py` carried its own **second CRC-16** — a
    private `_crc16()` beside `doppler.wfm.crc16`, the C kernel the source
    already appends with — plus a hand XOR-spread of the frame and a
    hand-tiled preamble, all flattened into a `"type": "bits"` pattern. It is
    now one `"type": "dsss"` scene segment naming the two codes, the repeat
    count, the sync word and the payload; the engine spreads, appends the CRC
    and derives the segment's own length. Cross-checked on adoption: the six
    decoded bursts come out **bit-identical** — same test statistics, code
    phases, recovered Doppler and SNR — so the hand copy and the kernel
    agreed, which is the only reason this was invisible.

    `dsss_receiver_demo.py` built its continuous asynchronous signal as a
    numpy expression (`data[si] * _CSIGN[cph] * exp(...)`); it is now
    `Synth(type="dsss", symbol_rate=…)`, the same call its sibling
    `async_dsss_receiver_spec_demo.py` already used.

    Ground truth in both now comes from `bpsk_map`, the C kernel the source
    maps bits with, rather than a sign convention restated in the example.
    That is a **fix**, not a tidy-up: both files returned the *negation* of
    what was transmitted and were saved only by their BER scorers searching
    inversion, so every decode reported `inverted=True` for a signal that was
    not inverted.

- **`dsss_burst_demo.py`'s payload is now actually spread, and a gate says
    so.** Its headline claim — *"the occupied bandwidth is identical
    throughout: a DSSS burst looks like noise from start to silence"* — was
    false. The "payload" was a `type="bpsk"` segment at one long rectangular
    pulse per bit (`sps` set to a whole code period), not a code at all.
    Measured at the demo's own 18 dB: the preamble filled **0.69** of the band
    and the payload **0.010**, a 70× discrepancy that panel C plotted and
    nothing checked.

    The burst is now one `Segment(type="dsss")` with a second, independent
    31-chip MLS spreading the payload — preamble 0.69, payload 0.53. The
    assertion added beside it is ratio-based (`occ_pay / occ_acq > 0.5`), with
    the bar taken from what the measurement separates: 0.77 spread against
    0.014 unspread, ~35× either side. Sabotage-proven against the exact
    construction the file shipped. The acquisition peak / payload floor also
    improves 16.6× now that the payload rides a different code.
