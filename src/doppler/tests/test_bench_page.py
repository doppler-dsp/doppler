"""The benchmarks page's data, labels and drift check (doppler#2372).

`scripts/bench_report.py --page` turns every published snapshot into one
quantity, seconds per unit, so a row's trend is continuous across releases
that recorded it differently (``MSa_s`` before v0.43.0, a per-round mean
over ``iterations`` after). These cases seed a small tree -- snapshots, a
jm manifest, a label table -- and check what the page is built from:

- a value is seconds per unit in every release, and the trend BREAKS where
  a C row's round changed size, instead of drawing a jump that is not real;
- only rows the newest release measures are shown; a retired row is gone;
- a row's family comes from the manifest's module lists;
- the label gate refuses an unlabelled row, a label without a field or for
  a dead row, a stale exempt line, a unit that contradicts the snapshot,
  and an exempt line the merge base did not hold;
- ``--check`` fails once the committed page no longer matches.

acc_f32::step is the row #2314 was about: 65,536 calls of ``step()`` per
round, 39.08 µs a round, so 0.596 ns a call.
"""

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPTS = repo_root() / "scripts"
ROUND_S = 39.08e-6
ITERS = 65536

MANIFEST = """\
[module.accumulator]
objects = ["acc_f32"]

[module.track]
objects = ["costas"]
"""


def _load():
    """Import `scripts/bench_report.py` by path (scripts/ is no package)."""
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            "bench_report", SCRIPTS / "bench_report.py"
        )
        assert spec and spec.loader
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))


def _c(name: str, mean: float, iters: int, extra=None) -> dict:
    return {
        "name": name,
        "fullname": "bench_" + name.replace("::", "_core::", 1),
        "extra_info": extra or {},
        "stats": {"mean": mean, "iterations": iters},
    }


def _tree(tmp_path: Path, releases: dict, labels="", exempt=None) -> Path:
    """A repo-shaped tree: {version: [C rows]} published, both builds."""
    for v, rows in releases.items():
        d = tmp_path / "benchmarks" / "published" / v
        d.mkdir(parents=True)
        for build in ("portable", "native"):
            (d / f"{build}.json").write_text(
                json.dumps({"machine_info": {"cpu": "x"}, "benchmarks": []}),
                encoding="utf-8",
            )
            (d / f"{build}-c.json").write_text(
                json.dumps({"benchmarks": rows}), encoding="utf-8"
            )
    (tmp_path / "just-makeit.toml").write_text(MANIFEST, encoding="utf-8")
    (tmp_path / "benchmarks" / "labels.toml").write_text(
        labels, encoding="utf-8"
    )
    if exempt is not None:
        (tmp_path / "benchmarks" / ".labels-exempt").write_text(
            "".join(f"{e}\n" for e in exempt), encoding="utf-8"
        )
    return tmp_path


def _row(data: dict, rid: str) -> dict:
    (row,) = [r for r in data["rows"] if r["id"] == rid]
    return row


def test_every_release_becomes_seconds_per_unit(tmp_path: Path) -> None:
    """MSa_s (old sets) and mean / iterations (new ones) agree."""
    msas = ITERS / ROUND_S / 1e6
    tree = _tree(
        tmp_path,
        {
            "v0.37.3": [_c("acc_f32::step", ROUND_S, ITERS, {"MSa_s": msas})],
            "v0.71.0": [_c("acc_f32::step", ROUND_S, ITERS)],
        },
    )
    row = _row(_load().page_data(str(tree)), "c:acc_f32::step")
    assert row["p"] == [pytest.approx(5.963e-10, rel=1e-3)] * 2
    assert row["brk"] == []


def test_a_round_that_changes_size_breaks_the_trend(tmp_path: Path) -> None:
    tree = _tree(
        tmp_path,
        {
            "v0.60.0": [_c("costas::steps", 1e-3, 4096)],
            "v0.61.0": [_c("costas::steps", 1e-3, 4096)],
            "v0.62.0": [_c("costas::steps", 16e-3, 65536)],
        },
    )
    row = _row(_load().page_data(str(tree)), "c:costas::steps")
    assert row["brk"] == [2]
    assert row["it"] == 65536


def test_a_retired_row_is_dropped(tmp_path: Path) -> None:
    tree = _tree(
        tmp_path,
        {
            "v0.60.0": [
                _c("costas::steps", 1e-3, 4096),
                _c("costas::old", 1e-3, 4096),
            ],
            "v0.61.0": [_c("costas::steps", 1e-3, 4096)],
        },
    )
    ids = [r["id"] for r in _load().page_data(str(tree))["rows"]]
    assert ids == ["c:costas::steps"]


def test_the_family_comes_from_the_manifest(tmp_path: Path) -> None:
    tree = _tree(
        tmp_path,
        {
            "v0.71.0": [
                _c("acc_f32::step", ROUND_S, ITERS),
                _c("costas::steps", 1e-3, 4096),
            ]
        },
    )
    data = _load().page_data(str(tree))
    assert _row(data, "c:acc_f32::step")["fam"] == "data"
    assert _row(data, "c:costas::steps")["fam"] == "track"


def test_a_component_nothing_claims_is_refused(tmp_path: Path) -> None:
    tree = _tree(
        tmp_path,
        {"v0.71.0": [_c("mystery::run", 1e-3, 1)]},
        exempt=["c:mystery::run"],
    )
    bad = _load().label_problems(str(tree))
    assert any("no family" in b for b in bad), bad


LABEL = """\
["c:costas::steps"]
name = "Costas carrier loop"
unit = "sample"
definition = "A Costas loop, per input sample."
headline = true
"""


def test_an_unlabelled_row_fails_and_an_exempt_one_passes(
    tmp_path: Path,
) -> None:
    rel = {
        "v0.71.0": [
            _c("costas::steps", 1e-3, 4096),
            _c("acc_f32::step", ROUND_S, ITERS),
        ]
    }
    br = _load()
    bad = br.label_problems(str(_tree(tmp_path / "a", rel, LABEL)))
    assert any("`c:acc_f32::step` has no label" in b for b in bad), bad
    ok = _tree(tmp_path / "b", rel, LABEL, exempt=["c:acc_f32::step"])
    assert br.label_problems(str(ok)) == []


@pytest.mark.parametrize(
    ("labels", "exempt", "said"),
    [
        (LABEL.replace('name = "Costas carrier loop"\n', ""), [], "no name"),
        (
            LABEL + '\n["c:gone::row"]\nname = "x"\nunit = "call"\n'
            'definition = "x"\n',
            [],
            "is no live row",
        ),
        (LABEL, ["c:costas::steps"], "labelled now"),
        (LABEL, ["c:gone::row"], "no live row -- remove the line"),
        (LABEL.replace('"sample"', '"call"'), [], "but the snapshot records"),
    ],
)
def test_the_label_gate_refuses(
    tmp_path: Path, labels: str, exempt: list, said: str
) -> None:
    rel = {
        "v0.71.0": [
            _c("costas::steps", 1e-3, 4096, {"unit": "sample", "MSa_s": 4.096})
        ]
    }
    bad = _load().label_problems(str(_tree(tmp_path, rel, labels, exempt)))
    assert any(said in b for b in bad), bad


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-C", str(root), *args], check=True, capture_output=True
    )


def test_the_exempt_list_may_not_grow_past_its_base(tmp_path: Path) -> None:
    rel = {
        "v0.71.0": [
            _c("costas::steps", 1e-3, 4096),
            _c("acc_f32::step", ROUND_S, ITERS),
        ]
    }
    tree = _tree(tmp_path, rel, "", exempt=["c:costas::steps"])
    _git(tree, "init", "-q", "-b", "main")
    _git(tree, "-c", "user.email=t@t", "-c", "user.name=t", "add", "-A")
    _git(
        tree,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    (tree / "benchmarks" / ".labels-exempt").write_text(
        "c:costas::steps\nc:acc_f32::step\n", encoding="utf-8"
    )
    bad = _load().label_problems(str(tree), base="main")
    assert any("`c:acc_f32::step` was added" in b for b in bad), bad


def test_check_catches_a_stale_page(tmp_path: Path) -> None:
    rel = {"v0.71.0": [_c("costas::steps", 1e-3, 4096)]}
    tree = _tree(tmp_path, rel, LABEL)
    (tree / "docs").mkdir()
    br = _load()
    assert br.cmd_page("docs/benchmarks.md", root=str(tree)) == 0
    assert br.cmd_page("docs/benchmarks.md", check=True, root=str(tree)) == 0
    page = tree / "docs" / "benchmarks.md"
    assert "Costas carrier loop" in page.read_text(encoding="utf-8")
    page.write_text("# Benchmarks\n", encoding="utf-8")
    assert br.cmd_page("docs/benchmarks.md", check=True, root=str(tree)) == 1
