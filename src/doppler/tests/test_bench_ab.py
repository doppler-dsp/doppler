"""`make bench-ab`'s verdicts: the gate a performance PR is held to (#2759).

The worktrees, builds and timed passes need a bench box and minutes, so they
are exercised by hand (the issue's "reproduces #2744" check). What is pinned
here is the part that decides pass or fail, on synthetic snapshots shaped
like `make bench`'s own JSON: the per-pass best round, the tie rule, and
which verdicts fail the run. The module's doctests run too, so a docstring
example cannot drift from the code.
"""

import doctest
import importlib.util
import os
import subprocess
import sys

import pytest

from doppler.tests._repo import repo_root

SCRIPTS = repo_root() / "scripts"


def _load():
    """bench_ab and the bench_interleaved it imported, by path.

    Every module the import adds to `sys.modules` (bench_interleaved,
    bench_report, ...) is removed again. Left there, it is what
    `inspect.getmodule` resolves those NAMES to, so another test's own
    by-path copy of the same script stops matching its functions and
    `doctest` finds nothing in it: test_bench_core_pinning's doctest runs
    read "attempted 0" on every Python leg of train #2772.
    """
    before = set(sys.modules)
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            "bench_ab", SCRIPTS / "bench_ab.py"
        )
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod, sys.modules["bench_interleaved"]
    finally:
        sys.path.remove(str(SCRIPTS))
        for name in set(sys.modules) - before:
            del sys.modules[name]


AB, BI = _load()


def _snap(rows):
    """A `make bench` snapshot: {fullname: best round, seconds}."""
    return {
        "benchmarks": [
            {"fullname": k, "stats": {"min": v}} for k, v in rows.items()
        ]
    }


def _run(a_passes, b_passes):
    return AB.compare(
        AB.best_rounds([_snap(p) for p in a_passes]),
        AB.best_rounds([_snap(p) for p in b_passes]),
    )


def test_the_module_doctests_pass():
    failed, tried = doctest.testmod(AB)
    assert tried > 0 and failed == 0


def test_noise_within_the_spread_is_a_tie():
    """Same code, run-to-run noise: every row ties, nothing fails."""
    a = [{"r": 1.00, "s": 5.0}, {"r": 1.03, "s": 5.1}, {"r": 1.01, "s": 4.9}]
    b = [{"r": 1.02, "s": 5.0}, {"r": 1.00, "s": 5.2}, {"r": 1.04, "s": 4.95}]
    res = _run(a, b)
    assert {v["verdict"] for v in res.values()} == {"TIE"}
    assert AB.failures(res) == []


def test_one_row_slowed_in_b_fails_that_row_only():
    """The sabotage the issue asks for: one row 10% slower in B, every
    other row unchanged. That row, and only it, fails."""
    a = [{"r": 1.00, "s": 5.0}, {"r": 1.01, "s": 5.01}, {"r": 1.00, "s": 5.0}]
    b = [{"r": 1.10, "s": 5.0}, {"r": 1.11, "s": 5.01}, {"r": 1.10, "s": 5.0}]
    res = _run(a, b)
    assert res["r"]["verdict"] == "SLOWER"
    assert res["s"]["verdict"] == "TIE"
    assert AB.failures(res) == ["r"]


def test_a_faster_b_passes():
    a = [{"r": 2.0}, {"r": 2.01}]
    b = [{"r": 1.0}, {"r": 1.01}]
    res = _run(a, b)
    assert res["r"]["verdict"] == "FASTER" and AB.failures(res) == []


def test_a_row_b_never_measured_fails():
    """A gate cannot pass on a row it never saw: B dropping one is red."""
    res = _run([{"r": 1.0, "gone": 1.0}] * 2, [{"r": 1.0}] * 2)
    assert res["gone"]["verdict"] == "MISSING"
    assert AB.failures(res) == ["gone"]


def test_a_new_row_in_b_is_reported_not_failed():
    res = _run([{"r": 1.0}] * 2, [{"r": 1.0, "added": 3.0}] * 2)
    assert res["added"]["verdict"] == "NEW"
    assert AB.failures(res) == []


def test_the_tie_rule_uses_the_larger_band():
    """A noisy A cannot be beaten by a quiet B within A's own noise."""
    v = AB.verdict([1.0, 1.5], [1.4, 1.4])
    assert v["band"] == 0.5 and v["tol"] == 0.5 and v["verdict"] == "TIE"


def test_one_outlier_pass_cannot_hide_a_regression():
    """#2771: A's one slow pass (3.0) widened a max - min band to 2.0, and
    B's 50% regression (1.5 against 1.0) read as a TIE. The band is the
    gap between the two best passes, so the outlier counts for nothing.
    Sabotage: take the band as max - min -> TIE, red."""
    res = _run(
        [{"r": 1.0}, {"r": 1.0}, {"r": 1.0}, {"r": 1.0}, {"r": 3.0}],
        [{"r": 1.5}] * 5,
    )
    assert res["r"]["verdict"] == "SLOWER"
    assert AB.failures(res) == ["r"]
    assert res["r"]["passes_a"] == [1.0, 1.0, 1.0, 1.0, 3.0]


def test_a_checkouts_bias_under_the_floor_is_a_tie():
    """A null A/A run of one commit read every B pass 0.54% above A's best
    (a steady bias between two checkouts). Under the 1% floor that is a
    TIE; 2% steady is SLOWER. Sabotage: FLOOR = 0 -> the bias is red."""
    a = [{"r": 89477.8}, {"r": 89558.8}, {"r": 89705.1}]
    b = [{"r": 89961.0}, {"r": 90078.4}, {"r": 90199.2}]
    assert _run(a, b)["r"]["verdict"] == "TIE"
    slow = [{"r": v["r"] * 1.02} for v in a]
    assert _run(a, slow)["r"]["verdict"] == "SLOWER"


# ── bench_once counts only what its own pass wrote (#2771) ──────────────


def _write(path, mtime):
    path.write_text('{"benchmarks": []}')
    os.utime(path, (mtime, mtime))


def test_a_pass_that_wrote_nothing_new_is_refused(tmp_path):
    """A previous pass's snapshot must not be counted again: it would
    shrink the band and read as a measurement. Sabotage: drop the mtime
    filter -> the old file is returned, red."""
    _write(tmp_path / "old-c.json", 1000.0)
    with pytest.raises(SystemExit, match="wrote no new snapshot"):
        BI.fresh_snapshots(str(tmp_path), since=2000.0)


def test_a_c_only_pass_returns_its_own_c_snapshot(tmp_path):
    _write(tmp_path / "old-c.json", 1000.0)
    _write(tmp_path / "new-c.json", 3000.0)
    assert BI.fresh_snapshots(str(tmp_path), since=2000.0) == (
        None,
        str(tmp_path / "new-c.json"),
    )


def test_a_python_snapshot_is_paired_with_its_own_c_one(tmp_path):
    _write(tmp_path / "t1.json", 3000.0)
    _write(tmp_path / "t1-c.json", 3000.0)
    _write(tmp_path / "t0-c.json", 2500.0)  # newer than since, other pass
    assert BI.fresh_snapshots(str(tmp_path), since=2000.0) == (
        str(tmp_path / "t1.json"),
        str(tmp_path / "t1-c.json"),
    )


def test_a_python_snapshot_without_its_c_one_is_refused(tmp_path):
    _write(tmp_path / "t1.json", 3000.0)
    with pytest.raises(SystemExit, match="no C snapshot"):
        BI.fresh_snapshots(str(tmp_path), since=2000.0)


def test_a_python_only_pass_returns_its_python_snapshot_alone(tmp_path):
    """`jm bench --python-only` writes no C snapshot, so a Python-only
    shard of a two-box run is the Python file alone. Sabotage: drop the
    python_only early return -> "no C snapshot", red."""
    _write(tmp_path / "t0.json", 1000.0)  # a previous pass's
    _write(tmp_path / "t1.json", 3000.0)
    assert BI.fresh_snapshots(
        str(tmp_path), since=2000.0, python_only=True
    ) == (str(tmp_path / "t1.json"), None)


def test_a_python_only_pass_that_wrote_nothing_is_refused(tmp_path):
    _write(tmp_path / "t0.json", 1000.0)
    with pytest.raises(SystemExit, match="wrote no new snapshot"):
        BI.fresh_snapshots(str(tmp_path), since=2000.0, python_only=True)


def test_a_python_snapshot_with_a_stale_c_one_is_refused(tmp_path):
    """Its C twin exists but a previous pass wrote it."""
    _write(tmp_path / "t1.json", 3000.0)
    _write(tmp_path / "t1-c.json", 1000.0)
    with pytest.raises(SystemExit, match="no C snapshot"):
        BI.fresh_snapshots(str(tmp_path), since=2000.0)


def test_a_failed_build_removes_its_worktree(monkeypatch, capsys):
    """setup_worktree cleans up after `make pyext` fails, and says why."""
    calls = []

    class R:
        def __init__(self, rc, err=""):
            self.returncode, self.stderr, self.stdout = rc, err, ""

    def fake_run(cmd, cwd, **kw):
        calls.append(cmd)
        if cmd[0] == "make":
            return R(2, "the compiler said no")
        return R(0)

    monkeypatch.setattr(BI, "_run", fake_run)
    with pytest.raises(SystemExit, match="failed for abc1234"):
        BI.setup_worktree("portable", "abc1234", wt="/tmp/x-wt")
    after = calls[[c[0] for c in calls].index("make") + 1 :]
    assert ["git", "worktree", "remove", "--force", "/tmp/x-wt"] in after
    assert "the compiler said no" in capsys.readouterr().err


# ── the refusals and the exit code of main() (#2771) ────────────────────


def _git(cwd, *args):
    return subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=cwd,
        check=True,
        text=True,
        capture_output=True,
    ).stdout.strip()


def test_a_dirty_head_is_refused(tmp_path, monkeypatch):
    """B=HEAD with an uncommitted edit would measure the committed tree."""
    _git(tmp_path, "init", "-q")
    (tmp_path / "f").write_text("1")
    _git(tmp_path, "add", "f")
    _git(tmp_path, "commit", "-q", "-m", "x")
    head = _git(tmp_path, "rev-parse", "--short", "HEAD")
    monkeypatch.setattr(AB, "REPO", str(tmp_path))
    assert AB.dirty_head("HEAD", head) == ""
    (tmp_path / "f").write_text("2")
    assert "uncommitted" in AB.dirty_head("HEAD", head)
    assert AB.dirty_head("@", head) != ""  # @ spells the checkout too
    assert AB.dirty_head("HEAD", "0000000") == ""  # another commit: fine
    # A name means its commit, even HEAD's: A=main on a dirty main, or the
    # full sha, is a fair question.
    branch = _git(tmp_path, "rev-parse", "--abbrev-ref", "HEAD")
    full = _git(tmp_path, "rev-parse", "HEAD")
    assert AB.dirty_head(branch, head) == ""
    assert AB.dirty_head(full, head) == ""


def _fake_main(monkeypatch, a_rows, b_rows):
    """main() over canned passes, no worktree, build or bench run."""
    monkeypatch.setattr(AB, "_resolve", lambda ref: ref)
    monkeypatch.setattr(AB, "dirty_head", lambda ref, c: "")
    monkeypatch.setattr(AB, "machine_not_ready", lambda: [])
    monkeypatch.setattr(AB, "fastest_cpus", lambda: None)
    monkeypatch.setattr(AB, "_remove_worktrees", lambda wts: None)
    monkeypatch.setattr(AB, "worktree_path", lambda side: side)
    monkeypatch.setattr(AB, "setup_worktree", lambda b, c, wt, **k: wt)
    rows = {"A": iter(a_rows), "B": iter(b_rows)}
    monkeypatch.setattr(
        AB,
        "bench_once",
        lambda wt, cpus: ({"benchmarks": []}, _snap(next(rows[wt]))),
    )


def test_main_exits_0_on_a_tie_and_1_on_a_slower_row(monkeypatch):
    _fake_main(monkeypatch, [{"r": 1.0}] * 3, [{"r": 1.0}] * 3)
    assert AB.main(["A", "B", "-k", "3"]) == 0
    _fake_main(monkeypatch, [{"r": 1.0}] * 3, [{"r": 1.5}] * 3)
    assert AB.main(["A", "B", "-k", "3"]) == 1


def test_main_exits_1_when_b_never_measured_a_row(monkeypatch):
    _fake_main(monkeypatch, [{"r": 1.0, "x": 1.0}] * 3, [{"r": 1.0}] * 3)
    assert AB.main(["A", "B", "-k", "3"]) == 1


def test_main_refuses_k_under_3(monkeypatch):
    """At K = 2 the band is max - min again, so one outlier sets it."""
    _fake_main(monkeypatch, [{"r": 1.0}] * 2, [{"r": 1.0}] * 2)
    assert AB.main(["A", "B", "-k", "2"]) == 2


def test_main_exits_2_when_a_ref_is_a_dirty_head(monkeypatch, capsys):
    """main() acts on dirty_head's reason, before building anything."""
    _fake_main(monkeypatch, [{"r": 1.0}] * 3, [{"r": 1.0}] * 3)
    built = []
    monkeypatch.setattr(
        AB, "dirty_head", lambda ref, c: "dirty" if ref == "B" else ""
    )
    monkeypatch.setattr(
        AB, "setup_worktree", lambda b, c, wt, **k: built.append(wt) or wt
    )
    assert AB.main(["A", "B", "-k", "3"]) == 2
    assert built == []
    assert "dirty" in capsys.readouterr().out


# ── A family renamed between A and B (#2900) ───────────────────────────────
#
# corr2d:: rows on main are correlator:: rows on the branch.
# benchmarks/renames.toml pairs them suffix for suffix, and a governed row
# that does not pair fails.

RENAMES = {"c:corr2d": "c:correlator"}


def _renamed(monkeypatch, table=None):
    monkeypatch.setattr(
        AB, "read_renames", lambda root: RENAMES if table is None else table
    )


def test_a_renamed_family_pairs_through_the_table(monkeypatch, capsys):
    """Each A row is judged against B's row of the same suffix, and the
    verdict says which A name it was measured under."""
    a = {"corr2d::x": 1.0, "corr2d::y": 1.0, "fft::z": 1.0}
    b = {"correlator::x": 1.0, "correlator::y": 1.0, "fft::z": 1.0}
    _fake_main(monkeypatch, [a] * 3, [b] * 3)
    _renamed(monkeypatch)
    assert AB.main(["A", "B", "-k", "3"]) == 0
    out = capsys.readouterr().out
    assert "A c:corr2d::<row> is B c:correlator::<row>" in out
    assert "c:correlator::x <- c:corr2d::x" in out
    assert "3 tie" in out


def test_a_renamed_row_slower_in_b_still_fails(monkeypatch):
    """The rename pairs; it does not forgive."""
    _fake_main(
        monkeypatch, [{"corr2d::x": 1.0}] * 3, [{"correlator::x": 1.5}] * 3
    )
    _renamed(monkeypatch)
    assert AB.main(["A", "B", "-k", "3"]) == 1


@pytest.mark.parametrize(
    ("a", "b", "row"),
    [
        # B dropped one renamed row: A only.
        ({"corr2d::x": 1.0, "corr2d::y": 1.0}, {"correlator::x": 1.0}, "y"),
        # B grew a row in the renamed family that A never had: B only.
        # Unrenamed it would be NEW, which passes; renamed it must pair.
        (
            {"corr2d::x": 1.0},
            {"correlator::x": 1.0, "correlator::w": 1.0},
            "w",
        ),
    ],
)
def test_a_renamed_row_on_one_side_only_is_refused(
    monkeypatch, capsys, a, b, row
):
    # A row both sides share, so the run reaches its verdicts whatever the
    # rename pairs.
    a, b = {**a, "fft::z": 1.0}, {**b, "fft::z": 1.0}
    _fake_main(monkeypatch, [a] * 3, [b] * 3)
    _renamed(monkeypatch)
    assert AB.main(["A", "B", "-k", "3"]) == 1
    out = capsys.readouterr().out
    assert f"c:correlator::{row}" in out
    assert "UNPAIRED" in out


def test_an_undeclared_rename_shows_as_missing_and_new(monkeypatch, capsys):
    """With no table entry, the renamed family is MISSING on A's side and
    NEW on B's, and the run fails: a rename is never paired by
    resemblance."""
    a = {"corr2d::x": 1.0, "fft::z": 1.0}
    b = {"correlator::x": 1.0, "fft::z": 1.0}
    _fake_main(monkeypatch, [a] * 3, [b] * 3)
    _renamed(monkeypatch, {})
    assert AB.main(["A", "B", "-k", "3"]) == 1
    out = capsys.readouterr().out
    assert "1 missing" in out and "1 new" in out
    assert "c:corr2d::x" in out and "c:correlator::x" in out


def test_the_rows_make_bench_writes_pair_by_row_id():
    """Real rows carry a ``name`` and a ``fullname``. C rows pair by the
    page's ID, not the bench binary; a Python row keeps its ID when its
    bench file moves to another module (step 4 moves the correlator's)."""

    def row(name, full, m):
        return {"name": name, "fullname": full, "stats": {"min": m}}

    a = {
        "c": [{"benchmarks": [row("corr2d::x", "bench_corr2d_core::x", 1.0)]}]
        * 3,
        "py": [
            {
                "benchmarks": [
                    row(
                        "bench_correlator::test_t",
                        "src/doppler/spectral/benchmarks/"
                        "bench_correlator.py::test_t",
                        1.0,
                    )
                ]
            }
        ]
        * 3,
    }
    b = {
        "c": [
            {
                "benchmarks": [
                    row("correlator::x", "bench_correlator_core::x", 1.0)
                ]
            }
        ]
        * 3,
        "py": [
            {
                "benchmarks": [
                    row(
                        "bench_correlator::test_t",
                        "src/doppler/correlation/benchmarks/"
                        "bench_correlator.py::test_t",
                        1.0,
                    )
                ]
            }
        ]
        * 3,
    }
    r, bad = AB.judge({"A": a, "B": b}, RENAMES)
    assert bad == []
    assert r["c:correlator::x"]["verdict"] == "TIE"
    assert r["c:correlator::x"]["alias_from"] == "c:corr2d::x"
    assert r["py:correlator::test_t"]["verdict"] == "TIE"


def test_two_rows_the_table_makes_one_are_refused():
    """A row renamed onto one A already has would silently drop one."""
    *_, bad = AB.apply_renames(
        {"c:old::x": [1.0], "c:new::x": [1.0]},
        {"c:new::x": [1.0]},
        {"c:old": "c:new"},
    )
    assert any("makes one row, c:new::x" in p for p in bad), bad
