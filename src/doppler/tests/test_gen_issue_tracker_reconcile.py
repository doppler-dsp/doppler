"""The tier map's live reconcile, driven over seeded issue lists.

`scripts/gen_issue_tracker.py --reconcile` is what the daily
`.github/workflows/issues.yml` runs (`make issues-check`). Until doppler#1716
nothing ran it, and the map drifted to 66 open issues untiered and 3 closed
ones listed while every gate stayed green. Each case seeds a live list, built
offline from the committed map so the test needs no network, with one kind
of drift in it, so the case fails if the reconcile cannot see that kind.
"""

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "gen_issue_tracker.py"
MAP = REPO / "docs" / "dev" / "issue-tiers.toml"


def _gen():
    """Import the generator as a module -- the code path `make issues` runs."""
    spec = importlib.util.spec_from_file_location("_issue_tracker", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _mapped() -> list[dict]:
    """A live list that matches the committed map exactly."""
    issues = _gen().load_map()["issue"]
    return [{"number": int(n), "title": r["title"]} for n, r in issues.items()]


def _reconcile(tmp_path: Path, rows: list[dict]):
    live = tmp_path / "live.json"
    live.write_text(json.dumps(rows), encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--reconcile", "--live", str(live)],
        capture_output=True,
        text=True,
        cwd=REPO,
        env={"PATH": "/usr/bin:/bin"},  # no GITHUB_STEP_SUMMARY
    )


def test_a_live_list_matching_the_map_passes(tmp_path: Path) -> None:
    r = _reconcile(tmp_path, _mapped())
    assert r.returncode == 0, r.stdout + r.stderr
    assert "every one tiered" in r.stdout


def test_an_untiered_open_issue_fails(tmp_path: Path) -> None:
    """Filed and never tiered: the drift #1716 counted 66 of."""
    rows = [*_mapped(), {"number": 999999, "title": "seeded, untiered"}]
    r = _reconcile(tmp_path, rows)
    assert r.returncode == 1
    assert "open issue(s) with no tier" in r.stdout
    assert "#999999  seeded, untiered" in r.stdout


def test_a_row_for_a_closed_issue_fails(tmp_path: Path) -> None:
    """Closed but still listed: an open issue's row whose issue went away."""
    rows = _mapped()
    gone = rows.pop()["number"]
    r = _reconcile(tmp_path, rows)
    assert r.returncode == 1
    assert "naming a CLOSED issue" in r.stdout
    assert f"#{gone}" in r.stdout


def test_an_empty_live_list_is_a_failed_read(tmp_path: Path) -> None:
    """Nothing to compare against is not 'no drift'."""
    r = _reconcile(tmp_path, [])
    assert r.returncode == 1
    assert "came back empty" in r.stdout


def test_drift_names_both_kinds() -> None:
    untiered, stale = _gen().drift({1: "a", 3: "c"}, {"1": {}, "2": {}})
    assert (untiered, stale) == ([3], [2])


def test_write_drops_a_closed_row_and_keeps_every_open_why() -> None:
    """`make issues` resolves a closed issue's row with no human step.

    `refresh` is the part of `--write` that edits the map. Driven over the
    committed map with one tiered issue closed: that row goes, every open
    row keeps its tier and its `why`, and the title follows the live list.
    """
    gen = _gen()
    issues = gen.load_map()["issue"]
    before = {n: (r["tier"], r.get("why")) for n, r in issues.items()}
    live = {int(n): r["title"] for n, r in issues.items()}
    closed = max(live)
    del live[closed]
    some = min(live)
    live[some] = "retitled upstream"

    dropped = gen.refresh(issues, live, {})

    assert dropped == [closed]
    assert str(closed) not in issues
    assert sorted(issues, key=int) == sorted(map(str, live), key=int)
    for n, rec in issues.items():
        assert (rec["tier"], rec.get("why")) == before[n]
    assert issues[str(some)]["title"] == "retitled upstream"
    assert any(rec.get("why") for rec in issues.values())


def test_the_written_map_keeps_each_why(tmp_path: Path, monkeypatch) -> None:
    """A tier's reason survives `make issues` rewriting the map."""
    gen = _gen()
    out = tmp_path / "tiers.toml"
    monkeypatch.setattr(gen, "MAP", out)
    data = {
        "meta": {"generated": "2026-10-01"},
        "issue": {
            "7": {"tier": 1, "title": "t", "status": "open", "why": "w"},
            "8": {"tier": 5, "title": "u", "status": "open"},
        },
    }
    gen._write_map(data)
    text = out.read_text(encoding="utf-8")
    assert 'why = "w"' in text
    assert text.count("why =") == 1
