- **`DsssBurstReceiver`** (new, C) — the burst chain composed in one object,
    the way `DsssReceiver` already composes the continuous one. **Declared and
    scaffolded; `push()` is not yet implemented** (see below), so this is the
    lifecycle the algorithm will hang on rather than a working receiver.

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
    - **A refine stage was missing.** A code-period correlation cannot name the
        repetition, but the whole preamble can — its finite extent breaks the
        periodicity — putting the argmax on the true start exactly, with the
        period-offset envelope following `(reps - abs(k)) / reps`. The chain is
        `search → refine → demod`.
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

    Serializable: the triplet checkpoints **between** bursts, which is what
    makes `BurstDemod`'s deliberate statelessness cost nothing.

    Tracked in [#1001](https://github.com/doppler-dsp/doppler/issues/1001).
