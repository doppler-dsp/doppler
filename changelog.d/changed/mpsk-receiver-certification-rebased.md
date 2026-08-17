- **`MpskReceiver`'s certification is re-based on measurements that can carry
    it** — 33 limits to 60, with a claim inventory (§1.1) mapping all 28
    header claims onto the C test, a report section, or C-ONLY.

    The Es/N0 grid is now **derived per M** from the bound and the record
    length. One grid across all three orders put BPSK where it makes no errors
    at all: six of nine cells could not bound anything, and one of them
    reported the receiver *beating* the matched-filter bound off three errors.
    Each M is now measured where its own bound predicts enough errors in the
    symbols actually **scored**, so the grid moves with the record length
    instead of being retyped, and section 4 asserts only over cells clearing a
    stated error floor.

    Four sections are new. **§2.8** halves `m_out` at fixed Es/N0: ~2.7 dB of
    EVM at every M, reproducing the header's QPSK figure and **not** its
    M-dependent ordering, which is anchored at an SER this record length
    cannot reach. **§2.9** measures the AGC's level law. **§2.10** covers
    lifecycle, telemetry and state, with the resume checked against a *warm*
    instance so the blob has to determine the continuation rather than merely
    not contradict it. **§2.7** grows a true-lock control and all three
    metrics, which turns the invisibility claim into evidence — and
    corroborates `docs/design/rx-test.md` §8.6's independent measurement to
    0.05 dB.

    Two findings are rewritten on the corrected bound, and both, plus
    [#781](https://github.com/doppler-dsp/doppler/issues/781), turn out to be
    the same mechanism: a loop that recovers the symbol rate to 2 ppm against
    a record the meter cannot align. **8PSK's implementation loss is
    unmeasurable by any per-push sweep**, because its measurable window
    (≤14.5 dB) and its working window (≥17 dB) do not overlap. A new finding
    collects six header claims a binding reaches that nothing measures
    ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    Every characterisation section now closes with what its table *means*,
    which is the half `CarrierNda`'s report had and this one did not.
