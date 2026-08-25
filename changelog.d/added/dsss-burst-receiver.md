- **`DsssBurstReceiver`** (new, C) — the burst chain composed in one object,
    the way `DsssReceiver` already composes the continuous one. Stream samples
    in; get one burst's payload bits out, with a CRC verdict and a detection
    event describing it. Three stages behind one `push()`:
    **search → refine → demod**.

    Every part of the burst chain was already certified — `BurstAcquisition`,
    `BurstDespreader`, `BurstDemod` — and nothing composed them, so the
    hand-off between them was arithmetic every caller redid in Python. What
    that cost is on the record: one bin→frequency fold restated in four call
    sites, three different ways.

    A prototype measured why the seam is hard, and the design
    (`docs/design/dsss-burst-receiver.md`) is built on it:

    - `code_phase` is a **residue**, exactly `burst_start mod code_period`. A
        window one whole code period early reports the *same* value, so a phase
        seed resolves alignment within a period and never *which* period.
    - An epoch error is recoverable in **one direction only**. `set_prior`'s
        `start` absorbs an early window completely (0/64 bit errors two periods
        early); nothing absorbs a late one (29/64, 37/64). The obligation is
        never to be late, not to be exact.
    - **A refine stage was missing**, and it is what recovers the exact
        preamble start. A code-period correlation cannot name the repetition;
        the preamble can, because its finite extent breaks the periodicity.
        Score each candidate by correlating one code period at every position
        the preamble would occupy and summing the **magnitudes** — the
        envelope follows `(reps - abs(k)) / reps` and peaks at the truth.
        Combine **non-coherently**: correlating all `reps * P` samples as one
        reference is the obvious form and does not survive the residual
        acquisition leaves — a quarter of a Doppler bin put the coherent peak
        two whole code periods off, with the true position 639x below it.
    - Timing is the fragile half: frequency pull-in measured 4 Doppler bins
        against a grid guaranteeing half a bin, while *one* code period of
        timing error is fatal.

    Consequently the receiver needs **look-back**, not the forward tail
    `DsssReceiver` derives: acquisition reports an *end* anchor and the burst
    start has already gone past. It retains a bounded history in the existing
    double-mapped ring (`buffer.h`, already an `acq` dependency — no new type),
    sized from the geometry rather than from a caller-supplied knob, because
    `acq_push()` consumes every frame it processes and has therefore released
    what the receiver still needs.

    Serializable, and `state_bytes()` is a pure function of configuration —
    both variable regions (the retained look-back and the acquisition child's
    own ring) are written into fixed-size areas with a length prefix. That is
    not cosmetic: jm's binding compares an incoming blob's length against
    `state_bytes()`, so a size that moved with the stream would make a
    receiver restorable only into an instance holding exactly as much
    history, which is coincidence rather than resume. Checkpointing
    **between** bursts is what makes `BurstDemod`'s deliberate statelessness
    cost nothing, and the retained look-back travels in the blob because the
    next burst's window may begin inside it.

    `refine_margin` reports the winning period over its nearest rival — near
    1 means the period was **not** resolved. It is the object's view of its
    own hand-off, and the gap it fills is real: with the window a period off,
    the carrier is still present, so a lock-style indicator reads healthy
    while the despread output is noise.

    Known limit, filed as
    [#1002](https://github.com/doppler-dsp/doppler/issues/1002): a burst at
    exactly half a coherent Doppler bin is not detected — the slow-time FFT's
    scalloping null. Raising SNR does not help, because `test_stat` saturates
    against the code's own sidelobe floor.

    Tracked in [#1001](https://github.com/doppler-dsp/doppler/issues/1001).
