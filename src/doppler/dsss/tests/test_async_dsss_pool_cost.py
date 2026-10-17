"""Fast twin of `characterization/async_dsss_pool/characterize.py`.

The characterization times seconds of signal and a design-point searcher,
which takes minutes; this runs each of its sections on a few pushes, so a
section cannot stop importing or stop running between deliberate runs. It
checks shape and sense, never a timing: the timings are the record's,
stamped with the machine that made them.
"""

from __future__ import annotations

from doppler.dsss.tests.characterization.async_dsss_pool.characterize import (
    TE,
    capture,
    s5_1_acquisition,
    s6_1_tracking,
    s6_2_search_depth,
    s7_1_split,
    s7_2_design_point,
)


def test_capture_is_whole_epochs():
    x = capture(seconds=0.01, code_only=False)
    assert x.size % TE == 0 and x.size >= 4 * TE


def test_acquisition_accounts_for_its_pushes():
    rows = s5_1_acquisition(esn0s=(10.0,), seconds=0.05)
    assert rows[0]["searching"] >= 1
    assert rows[0]["to_tracking_ms"] > 0


def test_tracking_reports_both_signals():
    # Long enough to reach tracking at 10 dB: 0.25 s is 750 epochs, and the
    # record's run tracks within a few hundred.
    rows = s6_1_tracking(esn0s=(10.0,), seconds=0.25)
    assert [r["signal"] for r in rows] == ["data on", "code only"]
    assert all(r["pushes"] > 0 and r["cpu"] > 0 for r in rows)


def test_search_depth_names_every_search():
    # A shallow window keeps the rate-bound row's depth small for the twin.
    rows = s6_2_search_depth(blocks=1, code_only_deep=3, min_pushes=0)
    assert [(r["range"], r["search"]) for r in rows] == [
        (rng, search)
        for rng in ("one tile", "+/-1/T_E")
        for search in (
            "the receiver's own",
            "slow-time, rate-free",
            "slow-time, rate-bound",
        )
    ]
    assert all(r["cpu"] > 0 and r["still_searching"] for r in rows)


def test_split_names_every_object():
    rows = s7_1_split(capture(seconds=0.01))
    assert [r["object"] for r in rows] == [
        "receiver",
        "pool",
        "searcher alone",
    ]


def test_design_point_runs_a_whole_dwell():
    rows = s7_2_design_point(blocks=1, counts=(1,), code_only=3)
    assert rows[0]["depth"] >= 1 and rows[0]["cpu"] > 0
