- **`CorrDetector2D` is certified** — 15 limits on every push. Three of the
    four noise modes (`MEDIAN`, `MIN`, `MAX`) were exercised by nothing in
    either language, and `noise_est` is the denominator of every decision this
    object makes: a mode returning the wrong statistic would move every
    `test_stat` in the library without moving a single peak position. Also
    covered: `set_ref`'s refusal branch, and the last-dump fields' promise to
    update regardless of threshold. Evidence:
    `src/doppler/spectral/tests/validation/detector2d/results.md`.
