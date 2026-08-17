- **`--check` gates a report's STRUCTURE, and the numbers are gated where they
    have units.** Byte-comparing `results.md` demanded reproducibility the
    numbers do not carry: measured across two toolchains (gcc 15.2/glibc 2.43
    against 13.3/2.39 on one CPU), a BPSK cell's error count moved 204 → 198 and
    SER, implementation loss and EVM moved with it — all inside the
    measurement's own ~7% standard error. Four of eleven reports were stale on
    the other machine, two of them for reasons predating any recent change
    ([#820](https://github.com/doppler-dsp/doppler/issues/820)).

    A tolerance was the obvious fix and does not work: absorbing the observed
    differences needs **>96%** relative, because relative deviation grows without
    bound as a quantity approaches zero and these reports deliberately measure
    quantities that converge to zero. At the point of comparison the artifact is
    markdown, so a `0.3` in a table has neither units nor provenance to key a
    per-quantity tolerance on.

    So `--check` masks numeric literals and byte-compares the rest — sections and
    their order, prose, each limit's claim wording, every verdict and finding
    tag, table shape. Section headings, `§N.M` references and `#N`/`gh-N`
    citations are **not** masked: they are structure written with digits, and an
    earlier version that masked them let a renumbered section and a re-pointed
    citation through. Numbers are gated by each object's
    `test_validation_limits.py`, which asserts them through the same `build()`
    with thresholds the author chose per quantity. Two gates, two questions.

    Verified against the toolchain that broke it: all eleven reports pass in a
    gcc 13.3 / glibc 2.39 container. **`make validate-check` is therefore wired
    into CI**, closing the half of #816 that was blocked on this.
