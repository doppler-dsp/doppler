- **`Plan` is certified** — the 30th object and wfm's last, a cache over the
    Composer, so its subject is INDISTINGUISHABILITY: 17 limits, 4 findings.
    Every override axis reproduces a full `compose()` bit for bit, and
    superposition holds to 5e-07. Two findings a caller should know: a
    ranged-gap scene draws a different length for every seed while
    `render()`'s docstring promises `len()`, which breaks both rectangular
    Monte-Carlo idioms ([#1128][gh1128]); and nothing can observe whether a
    restore took its cached fast path, so a silent fall back to full rebuild
    would pass every test in both suites ([#1129][gh1129]).
    [Evidence][plan-cert].

[gh1128]: https://github.com/doppler-dsp/doppler/issues/1128
[gh1129]: https://github.com/doppler-dsp/doppler/issues/1129
[plan-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_plan/results.md
