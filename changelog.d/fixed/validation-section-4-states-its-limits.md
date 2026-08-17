- **Section 4 of seven validation reports named none of the limits it
    counted.** `agc`, `ema`, `resamp`, `lockdet`, `mpsk`, `loop_filter` and
    `mpsk_receiver` each rendered the heading, the sentence "Claims a caller
    may rely on", and then nothing — while section 5 beside it closed with
    `N/N limits hold`. **262 certified claims that only four of eleven
    reports actually stated.**

    Neither gate could see it, for two different reasons.
    `test_validation_limits.py` asserts every limit and never reads the
    report. `make validate-check` re-renders and compares bytes, so a
    generator emitting an empty section agrees with itself perfectly — a
    staleness gate proves the artifact matches the generator, which says
    nothing about whether what is generated is complete. The passing tally
    sat beside the empty section, and the tally is what a reader trusts.

    The table is now emitted by `Report.summary()` — the one hook every
    validator calls, and the only one that runs after the last `limit()` —
    so it cannot be forgotten by a new object, and the four hand-rolled
    copies are gone rather than left to drift. `Report._self_check` refuses
    a render whose section 4 carries fewer rows than the run recorded,
    counted against the **rendered** text because that is the artifact a
    reader gets. Proven by sabotage, with three seeded cases in
    `test_validation_report.py` including the vacuity guard from the other
    side: a report asserting no limits needs no table.
