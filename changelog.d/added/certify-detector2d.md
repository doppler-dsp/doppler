- **`CorrDetector2D` is certified** —
    `src/doppler/spectral/tests/validation/detector2d/results.md`, 15 limits
    on every push, plus three C sections the suite was missing.

    **Three of the four noise modes were exercised by nothing.**
    `DET_NOISE_MEDIAN`, `DET_NOISE_MIN` and `DET_NOISE_MAX` had zero mentions
    in `test_detector2d_core.c` and zero in `test_detector2d.py`; only the
    mean was ever selected. `noise_est` is the **denominator of every
    decision this object makes**, so a mode returning the wrong statistic
    would have moved every `test_stat` in the library without changing a
    single peak position. Now measured in both languages against the
    aggregate computed by definition, over a window where the four modes
    genuinely disagree — a flat window would let any of them pass as any
    other. Sabotage-proven by forcing the estimator to the mean regardless
    of the configured mode.

    **`set_ref` had no coverage, and writing one found that the interesting
    branch is the refusal.** An impulse reference is single-row, so the
    embedded `Corr2D` is on its fast path, and that path can only accept
    another single-row reference — a multi-row replacement is *refused*.
    That is the documented contract working rather than a defect; the test
    now asserts both branches, including that a refusal is non-destructive
    and leaves the object running on its previous reference.

    **The last-dump fields are documented as updating "regardless of
    threshold", and getting a test that could actually fail took three
    attempts.** The two failures are the finding, and both are the vacuity
    this campaign keeps turning up:

    - asserting `_last_corr_valid == 1` after a gated push passes on the
        flag left set by the *previous* ungated push;
    - asserting `test_stat` alone passes because `compute_stat_2d` writes
        the peak position as a side effect, so a sabotage that restores only
        the statistic leaves the position correct.

    The check that discriminates pushes a peak at a **different position**
    under a closed gate and requires the reported position to follow it.
    Only a sabotage restoring all five last-dump fields takes it red.

    Also now pinned: the gate is **strict** (a threshold exactly equal to
    the statistic emits nothing, and `0.0` is the documented always-fire
    case rather than a very low threshold); the event sequence is identical
    across six chunk sizes down to one sample per `push`; and the default
    noise window is the whole surface, which is the `SIZE_MAX` sentinel the
    binding passes being clamped rather than propagated into the median
    scratch sizing.

    **Two entry points are C-ONLY and the report says so**: `set_ref`, and
    `set_threshold` — the `threshold` property is read-only from Python, so
    a caller wanting an adaptive threshold on a running detector must
    rebuild the object or drop to the C API.
