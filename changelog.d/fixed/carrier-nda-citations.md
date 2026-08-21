- **`carrier_nda_core.h` cited the wrong design section three times, and
    `make doc-sections-check` now catches that class.** All three pointed at
    `docs/design/mpsk.md` §2.3 — "The invariant", which is about rate-keyed
    constants — for the one-AGC-per-receiver argument, the squaring-loss
    measurement, and the lock statistic's H0 variance. All three arguments
    are in the document, in §3.2 and §4.2.

    That is not a cosmetic slip. #796's sibling issue was filed reporting
    that the `~6 dB Es/N0` floor "has no measurement behind it that I can
    find anywhere in the tree"; §3.2 carries a measured table of loop SNR
    against the un-normalised form, six Es/N0 rows by three constellations,
    4e5 samples per point. **A citation reads as authority, so one pointing
    at the wrong argument is worse than none — the reader concludes the
    claim is unsupported.**

    The gate checks that a `docs/x.md §N` citation names a section that
    exists — 81 of them across the tree, which nothing checked before — and,
    when the citation also names the section's title, that the title matches.
    The title half is what catches a wrong-but-existing number, which is
    every one of the three above; it is optional, so each citation that gains
    a title is coverage that cannot regress.

- **The amplitude note now separates scale from Es/N0.** It read as though
    section 9 of `test_carrier_nda_core.c` established that loop gain is
    independent of signal level. It does not, and cannot: it scales a clean
    phasor, holding signal and noise in one ratio. Per-sample division by the
    instantaneous `|s+n|` is a hard limiter, so the S-curve slope genuinely
    does depend on Es/N0 — which is measured, in the §3.2 table now cited.
    Section 9 keeps its place as a **float-range** gate: proven by sabotage,
    un-hoisting the divide makes it fail, because forming `|z|^M` at the end
    returns 0 below `|z| = 0.032` and NaN above 1e4 at M = 8.
