- **The gallery pages match what their examples run.** Twenty-odd pages
    quoted numbers and mechanisms that had drifted: measured values
    re-taken from each page's own script (DDC split transient 1.56, Q15
    coefficient peak 10348, the background-plan trim 10.01 dB, framed-link
    losses 2/200 and 10/200), the correction is Šidák, not Bonferroni
    (also in the `pfa_cell` docstrings of `Acquisition` and
    `BurstAcquisition`), the DLL discriminator is `0.5·(|E|²-|L|²)/|P|²`,
    and included regions are self-sufficient (#2582).
