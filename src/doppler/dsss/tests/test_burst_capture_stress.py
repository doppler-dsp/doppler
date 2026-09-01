"""Fast twin for the ``burst_capture`` characterization subject.

`make characterize` runs the full sweep — ten spacings at twelve trials each,
plus the scatter it builds from every window they emit — and that costs
minutes, so it is run deliberately rather than on every push. This is the
per-push cover: it imports the same helpers, the same geometry and the same
scene builder, so the subject cannot silently stop importing or stop working
while nobody is looking.

Be clear about the scope, which is the trade the characterization category
exists to make (see ``doppler.dsss.tests.characterization``): this proves the
helpers still run and that the two ENDS of the spacing envelope are still on
the right side of the floor. It does **not** re-derive the floor — a
regression that moves it from 256 samples to 512 without breaking an import
is caught by ``make characterize``, not here.
"""

import numpy as np

from doppler.dsss.tests.characterization.burst_capture.characterize import (
    BURST_LEN,
    REPS,
    SIGMA,
    _gaps,
    acq_code,
    burst,
    capture,
    run_pair,
    scene,
    separation,
)


def test_the_scene_builder_still_builds_a_burst():
    b = burst()
    assert b.dtype == np.complex64
    assert b.size == BURST_LEN
    # The preamble is REPS copies of one code period, so the first period
    # repeats exactly -- if it does not, the scene is not a DSSS burst.
    p = acq_code().size * 4  # code_period at spc=4
    assert np.array_equal(b[:p], b[p : 2 * p])

    x = scene([1000], 8000, seed=3, sigma=SIGMA)
    assert x.size == 8000
    assert np.abs(x[1000 : 1000 + p]).mean() > np.abs(x[:500]).mean()


def test_a_generous_gap_captures_both_bursts():
    """The top of the envelope: well past the floor, both come back."""
    found, _extra, rows = run_pair(gap=_gaps(capture().min_gap)[-1], seed=7)
    assert found == 2, "both transmitted bursts must be captured"
    real = [r for r in rows if r[2]]
    assert len(real) == 2
    # A resolved period sits at the (reps-1)/reps envelope, not near 1.
    envelope = (REPS - 1) / REPS
    assert all(r[0] < envelope + 0.15 for r in real)


def test_touching_bursts_are_the_hard_end():
    """The bottom of the envelope: at zero dead air the pair is not reliable.

    Asserted as "at most 2", not "fewer than 2" — the point is that the
    subject still RUNS at the hard end and reports a number, not that a
    particular trial fails. Pinning a failure would make a real improvement
    look like a regression.
    """
    found, _extra, _rows = run_pair(gap=0, seed=11)
    assert 0 <= found <= 2


def test_separation_reports_both_populations():
    rows = [(0.8, 57.0, True), (0.86, 48.0, False)]
    sep = separation(rows)
    assert sep["n_real"] == 1 and sep["n_spurious"] == 1
    # cn0 is the statistic that separates; the characterization measured
    # 57.0 dB-Hz at a real window against 48.3 at a spurious one.
    assert sep["cn0_real"] > sep["cn0_spurious"]


def test_the_object_under_it_is_the_real_one():
    cap = capture()
    assert cap.burst_len == BURST_LEN
    assert cap.retain_span == cap.refine_span + BURST_LEN
