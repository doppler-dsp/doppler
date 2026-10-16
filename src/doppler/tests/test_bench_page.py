"""The benchmarks page's data, labels and drift check (doppler#2372).

`scripts/bench_report.py --page` turns every published snapshot into one
quantity, seconds per unit, so a row's trend is continuous across releases
that recorded it differently (``MSa_s`` before v0.43.0, a per-round mean
over ``iterations`` after). These cases seed a small tree -- snapshots, a
jm manifest, a label table -- and check what the page is built from:

- a value is seconds per unit in every release, and the trend BREAKS where
  its value changed meaning (`breaks`: a unit, or before units a round
  size, or a Python rate key), instead of drawing a jump that is not real;
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
import re
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


def _py(test: str, mean: float, extra=None, iters: int = 1) -> dict:
    """A pytest-benchmark row of `bench_costas.py` (the track module)."""
    return {
        "name": f"bench_costas::{test}",
        "fullname": (f"src/doppler/track/benchmarks/bench_costas.py::{test}"),
        "extra_info": extra or {},
        "stats": {"mean": mean, "iterations": iters},
    }


def _tree(
    tmp_path: Path, releases: dict, labels="", exempt=None, py=None
) -> Path:
    """A repo-shaped tree: {version: [C rows]} published, both builds, and
    {version: [Python rows]} when ``py`` gives them."""
    for v, rows in releases.items():
        d = tmp_path / "benchmarks" / "published" / v
        d.mkdir(parents=True)
        for build in ("portable", "native"):
            pyrows = (py or {}).get(v, [])
            (d / f"{build}.json").write_text(
                json.dumps(
                    {"machine_info": {"cpu": "x"}, "benchmarks": pyrows}
                ),
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
samples = 1
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
            'definition = "x"\nsamples = 1\n',
            [],
            "is no live row",
        ),
        (LABEL, ["c:costas::steps"], "labelled now"),
        (LABEL, ["c:gone::row"], "no live row -- remove the line"),
        (LABEL.replace('"sample"', '"call"'), [], "but the snapshot records"),
        (LABEL.replace("samples = 1\n", ""), [], "declares no count"),
        (
            LABEL.replace("samples = 1", "samples = 4"),
            [],
            "counts 4 samples per unit",
        ),
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


def test_the_page_data_survives_instant_navigation(tmp_path: Path) -> None:
    """The data is a script with no attribute, read by one name (#2391).

    The site's instant navigation swaps a page in without a reload and
    re-creates each inline ``<script>`` in it with its text alone. The
    data used to be ``<script type="application/json" id="dp-bench-data">``:
    reached by a click, it ran as JavaScript, threw, and the page drew
    nothing -- while a reload drew everything. So the data is a script that
    sets ``window.<PAGE_DATA>``, the one name benchmarks.js reads, and no
    inline script on any docs page may lean on an attribute.
    """
    br = _load()
    tree = _tree(
        tmp_path, {"v0.71.0": [_c("costas::steps", 1e-3, 4096)]}, LABEL
    )
    page = br.render_page(str(tree))
    scripts = re.findall(r"<script\b([^>]*)>(.*?)</script>", page, re.S)
    assert [attrs for attrs, _ in scripts] == [""]
    head = f"window.{br.PAGE_DATA} = "
    body = scripts[0][1]
    assert body.startswith(head) and body.endswith(";")
    assert json.loads(body[len(head) : -1])["versions"] == ["v0.71.0"]

    js = (repo_root() / "docs" / "javascripts" / "benchmarks.js").read_text(
        encoding="utf-8"
    )
    assert len(re.findall(rf"=\s*window\.{br.PAGE_DATA}\s*;", js)) == 1

    for md in sorted((repo_root() / "docs").rglob("*.md")):
        text = md.read_text(encoding="utf-8")
        for attrs in re.findall(r"<script\b([^>]*)>", text):
            assert not attrs.strip() or "src=" in attrs, (str(md), attrs)


def test_a_stream_row_reads_in_samples_and_rs_in_bits(tmp_path: Path) -> None:
    """The page's rate is input samples per second, or the row's own domain:
    a frame counts its nfft, a codeword its label's 2,040 bits, and a getter
    is setup -- time per call, no rate."""
    label = (
        '["c:costas::decode[e=8]"]\nname = "RS-like"\nunit = "codeword"\n'
        'definition = "One codeword."\ndomain = "bit"\ncount = 2040\n'
    )
    rel = {
        "v0.71.0": [
            _c("costas::fft[nfft=256]", 1e-3, 64, {"unit": "frame"}),
            _c("costas::decode[e=8]", 1e-3, 64, {"unit": "codeword"}),
            _c("costas::get", 1e-3, 64, {"unit": "call"}),
        ]
    }
    data = _load().page_data(str(_tree(tmp_path, rel, label)))
    fft = _row(data, "c:costas::fft[nfft=256]")
    assert (fft["cnt"], fft["dom"], fft["inf"]) == (256, "Sa", True)
    rs = _row(data, "c:costas::decode[e=8]")
    assert (rs["cnt"], rs["dom"]) == (2040, "bit")
    assert _row(data, "c:costas::get")["dom"] is None


def test_a_read_out_is_time_per_call_not_a_rate(tmp_path: Path) -> None:
    """A read-out is no stream (#2372 review): a per-call row whose name
    carries an nfft is not nfft samples per call, and a read-out recorded
    per bin reads per call once its label says `domain = "call"`, with
    the call's time the bin's times the label's count."""
    label = (
        '["c:costas::value"]\nname = "Read-out"\nunit = "bin"\n'
        'definition = "One read-out of 4,096 bins."\n'
        'domain = "call"\ncount = 4096\n'
    )
    rel = {
        "v0.71.0": [
            _c("costas::power[nfft=1024]", 1e-6, 1, {"unit": "call"}),
            _c("costas::value", 4096e-10, 4096, {"unit": "bin"}),
        ]
    }
    tree = _tree(tmp_path, rel, label, exempt=["c:costas::power[nfft=1024]"])
    br = _load()
    data = br.page_data(str(tree))
    assert br.label_problems(str(tree)) == []
    power = _row(data, "c:costas::power[nfft=1024]")
    assert (power["dom"], power["cnt"]) == (None, 1)
    value = _row(data, "c:costas::value")
    assert (value["dom"], value["cnt"]) == (None, 4096)
    assert br._fmt_row(value, value["p"][-1]) == "409.60 ns / call"


def test_every_count_read_from_a_name_is_inferred(tmp_path: Path) -> None:
    """Only a label or a recorded unit states a count; the name does not."""
    rel = {
        "v0.71.0": [
            _c("costas::step", 1e-3, 4096),
            _c("costas::steps", 1e-3, 4096, {"unit": "sample"}),
            _c("costas::fft[nfft=256]", 1e-3, 64, {"unit": "frame"}),
        ]
    }
    data = _load().page_data(str(_tree(tmp_path, rel)))
    assert _row(data, "c:costas::step")["inf"] is True
    assert _row(data, "c:costas::fft[nfft=256]")["inf"] is True
    assert _row(data, "c:costas::steps")["inf"] is False


def test_a_legacy_unit_is_used_then_must_go(tmp_path: Path) -> None:
    """Before #2314 a C row records no unit; LEGACY_UNITS supplies it, and
    an entry whose row now records its own unit fails the gate."""
    seed = '"c:costas::steps" = "sample"\n'
    old = {"v0.71.0": [_c("costas::steps", 1e-3, 4096)]}
    tree = _tree(tmp_path / "old", old, "", exempt=["c:costas::steps"])
    (tree / "benchmarks" / "legacy-units.toml").write_text(seed, "utf-8")
    br = _load()
    row = _row(br.page_data(str(tree)), "c:costas::steps")
    assert (row["unit"], row["cnt"], row["inf"]) == ("sample", 1, False)
    assert br.label_problems(str(tree)) == []
    new = {"v0.73.0": [_c("costas::steps", 1e-3, 4096, {"unit": "sample"})]}
    tree = _tree(tmp_path / "new", new, "", exempt=["c:costas::steps"])
    (tree / "benchmarks" / "legacy-units.toml").write_text(seed, "utf-8")
    bad = br.label_problems(str(tree))
    assert any("records its own unit now" in b for b in bad), bad


def test_a_python_row_reads_in_the_unit_its_rate_names(
    tmp_path: Path,
) -> None:
    """A Python bench names its rate's unit in its key (#2373 review): a bit
    or chip generator that also wrote MSa_s produced no samples, and a
    receiver writing Msym_s beside MSa_s eats samples. A row with no rate
    reads per call; a label gives a bit stream its count per call."""
    label = (
        '["py:costas::test_bench_find_64k"]\nname = "Find"\nunit = "call"\n'
        'definition = "65,536 bits."\ndomain = "bit"\ncount = 65536\n'
    )
    py = [
        _py("test_bench_pn", 1e-4, {"MSa_s": 650.0, "Mbit_s": 650.0}),
        _py("test_bench_rx", 1e-4, {"MSa_s": 40.0, "Msym_s": 10.0}),
        _py("test_bench_gold", 1e-4, {"MSa_s": 650.0, "Mchip_s": 650.0}),
        _py("test_bench_find_64k", 8.7e-5),
    ]
    tree = _tree(
        tmp_path,
        {"v0.71.0": []},
        label,
        exempt=[
            "py:costas::test_bench_pn",
            "py:costas::test_bench_rx",
            "py:costas::test_bench_gold",
        ],
        py={"v0.71.0": py},
    )
    br = _load()
    assert br.label_problems(str(tree)) == []
    data = br.page_data(str(tree))
    pn = _row(data, "py:costas::test_bench_pn")
    assert (pn["unit"], pn["dom"], pn["inf"]) == ("bit", "bit", False)
    assert _row(data, "py:costas::test_bench_rx")["dom"] == "Sa"
    assert _row(data, "py:costas::test_bench_gold")["dom"] == "chip"
    find = _row(data, "py:costas::test_bench_find_64k")
    assert (find["cnt"], find["dom"]) == (65536, "bit")


def test_a_per_call_stream_row_reads_its_block_in_samples(
    tmp_path: Path,
) -> None:
    """A capture analysed per call is a stream (#2373 review): a label's
    `samples` is the block one call consumes, and the page reads it in
    samples per second, not under setup and control."""
    label = (
        '["c:costas::analyze[n=4096]"]\nname = "Analyze"\nunit = "call"\n'
        'definition = "One 4,096-sample capture."\nsamples = 4096\n'
    )
    rel = {
        "v0.71.0": [
            _c("costas::analyze[n=4096]", 2.0e-5, 1, {"unit": "call"}),
        ]
    }
    tree = _tree(tmp_path, rel, label)
    br = _load()
    assert br.label_problems(str(tree)) == []
    row = _row(br.page_data(str(tree)), "c:costas::analyze[n=4096]")
    assert (row["cnt"], row["dom"], row["inf"]) == (4096, "Sa", False)
    assert br._fmt_row(row, row["p"][-1]) == "205 MSa/s"


def test_since_last_is_computed_here_and_honours_a_break(
    tmp_path: Path,
) -> None:
    """ber_meter::align went from 65,536 iterations a round to 1 and read
    -100% since last, topping the sort (#2373 review). Across a break the
    two releases are different quantities: no change, and `slb` says why.
    """
    rel = {
        "v0.70.0": [
            _c("costas::align", 8e-4 * 65536, 65536),
            _c("costas::steps", 2e-3, 4096, {"unit": "sample"}),
        ],
        "v0.71.0": [
            _c("costas::align", 8e-4, 1),
            _c("costas::steps", 1e-3, 4096, {"unit": "sample"}),
        ],
    }
    data = _load().page_data(str(_tree(tmp_path, rel)))
    align = _row(data, "c:costas::align")
    assert (align["brk"], align["sl"], align["slb"]) == ([1], None, True)
    steps = _row(data, "c:costas::steps")
    assert (steps["sl"], steps["slb"]) == (1.0, False)


def test_a_hollow_point_is_dropped_not_drawn(tmp_path: Path) -> None:
    """py:corr read 203 GSa/s before v0.65.0 (#1918): past the coverage
    gate's ceiling a point is a hollow benchmark, so it is no point, and
    the since-last change is measured against the last real one."""
    py = {
        "v0.69.0": [_py("test_bench_execute_64k", 1.0, {"MSa_s": 2.0e5})],
        "v0.70.0": [_py("test_bench_execute_64k", 1.0, {"MSa_s": 250.0})],
        "v0.71.0": [_py("test_bench_execute_64k", 1.0, {"MSa_s": 250.0})],
    }
    rel = {v: [] for v in py}
    tree = _tree(
        tmp_path, rel, exempt=["py:costas::test_bench_execute_64k"], py=py
    )
    row = _row(
        _load().page_data(str(tree)), "py:costas::test_bench_execute_64k"
    )
    assert row["p"][0] is None
    assert row["p"][1:] == [pytest.approx(4e-9)] * 2
    assert (row["sl"], row["brk"]) == (0.0, [])


def test_the_page_prints_a_python_row_per_call_never_per_round(
    tmp_path: Path,
) -> None:
    """pytest-benchmark's mean is per call whatever its `iterations`
    says; #2360's case was a Python row with 220 iterations a round. The
    rendered headline table must read it per call."""
    label = (
        '["py:costas::test_bench_create"]\nname = "Create"\nunit = "call"\n'
        'definition = "One create."\ndomain = "call"\ncount = 1\n'
        "headline = true\n"
    )
    py = {"v0.71.0": [_py("test_bench_create", 2.288e-8, iters=220)]}
    tree = _tree(tmp_path, {"v0.71.0": []}, label, py=py)
    br = _load()
    assert br.label_problems(str(tree)) == []
    row = _row(br.page_data(str(tree)), "py:costas::test_bench_create")
    assert (row["it"], row["unit"]) == (1, "call")
    page = br.render_page(str(tree))
    (cell,) = [ln for ln in page.splitlines() if ln.startswith("| [Create]")]
    assert (
        cell == "| [Create](#d1) | 22.88 ns / call | 22.88 ns / call | +0.0% |"
    )
    assert "round" not in cell


def _commit(root: Path, msg: str) -> str:
    _git(root, "add", "-A")
    _git(
        root,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "--allow-empty",
        "-qm",
        msg,
    )
    return subprocess.run(
        ["git", "-C", str(root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def _stamp(root: Path, version: str, commit: str) -> None:
    """Record `commit` as the one `version` was measured at."""
    for build in ("portable", "native"):
        f = root / "benchmarks" / "published" / version / f"{build}.json"
        data = json.loads(f.read_text(encoding="utf-8"))
        data["doppler_meta"] = {"commit": commit}
        f.write_text(json.dumps(data), encoding="utf-8")


NEW_LABEL = (
    '\n["c:costas::steps[new]"]\nname = "New"\nunit = "sample"\n'
    'definition = "A row no release has published yet."\nsamples = 1\n'
)


def test_a_new_rows_label_waits_for_its_release(tmp_path: Path) -> None:
    """A row is labelled in the PR that adds it, before a release publishes
    it (#2373 review): the label is pending while the newest release was
    measured without it, and dead once a release measured with it records
    no such row."""
    rel = {"v0.71.0": [_c("costas::steps", 1e-3, 4096, {"unit": "sample"})]}
    tree = _tree(tmp_path, rel, LABEL)
    _git(tree, "init", "-q", "-b", "main")
    measured = _commit(tree, "v0.71.0 measured here")
    _stamp(tree, "v0.71.0", measured)
    (tree / "benchmarks" / "labels.toml").write_text(
        LABEL + NEW_LABEL, encoding="utf-8"
    )
    _commit(tree, "label the new row")
    br = _load()
    assert br.label_problems(str(tree)) == []
    # The next release is measured WITH the label, and has no such row.
    _stamp(tree, "v0.71.0", _commit(tree, "measured again"))
    bad = br.label_problems(str(tree))
    assert any("`c:costas::steps[new]` is no live row" in b for b in bad)


def test_the_fixer_deletes_what_the_gate_only_wants_deleted(
    tmp_path: Path,
) -> None:
    """v0.73.0 records every C row's unit, so every legacy-units entry goes
    stale at once (#2373 review). The gate says which command clears it,
    and the command deletes those lines -- and the file once it is empty
    -- plus every exempt line whose row is labelled or retired."""
    rel = {
        "v0.73.0": [
            _c("costas::steps", 1e-3, 4096, {"unit": "sample"}),
            _c("acc_f32::step", ROUND_S, ITERS, {"unit": "sample"}),
        ]
    }
    tree = _tree(
        tmp_path,
        rel,
        LABEL,
        exempt=[
            "# header",
            "c:costas::steps",
            "c:gone::row",
            "c:acc_f32::step",
        ],
    )
    legacy = tree / "benchmarks" / "legacy-units.toml"
    legacy.write_text(
        '# header\n"c:costas::steps" = "sample"\n"c:gone::row" = "call"\n',
        encoding="utf-8",
    )
    br = _load()
    bad = br.label_problems(str(tree))
    assert "4 of these only need a line deleted" in bad[-1], bad
    done = br.fix_labels(str(tree))
    assert len(done) == 5, done
    assert not legacy.exists()
    exempt = (tree / "benchmarks" / ".labels-exempt").read_text("utf-8")
    assert exempt == "# header\nc:acc_f32::step\n"
    assert br.label_problems(str(tree)) == []


def test_an_unreadable_release_commit_fails_the_gate_not_the_page(
    tmp_path: Path,
) -> None:
    """In a clone that cannot read the newest release's commit, a label
    for an unpublished row can be neither pending nor dead: the gate
    (`make lint`, full history) and the page writer fail closed, while the
    page check (the Docs job, a depth-1 checkout -- #2373's red Docs run)
    does not ask."""
    rel = {"v0.71.0": [_c("costas::steps", 1e-3, 4096, {"unit": "sample"})]}
    tree = _tree(tmp_path, rel, LABEL + NEW_LABEL)
    (tree / "docs").mkdir()
    _git(tree, "init", "-q", "-b", "main")
    _commit(tree, "labels")
    _stamp(tree, "v0.71.0", "0" * 40)
    br = _load()
    bad = br.label_problems(str(tree))
    assert any("cannot be read" in b for b in bad), bad
    # the writer (`make bench-docs`, on the bench box) runs the whole gate
    assert br.cmd_page("docs/benchmarks.md", root=str(tree)) == 1
    page = tree / "docs" / "benchmarks.md"
    page.write_text(br.render_page(str(tree)), encoding="utf-8")
    assert br.cmd_page("docs/benchmarks.md", check=True, root=str(tree)) == 0


def test_a_name_recorded_twice_keeps_its_first_value(tmp_path: Path) -> None:
    """acq records some rows once per Pd point under one name; the page
    keeps the first, for the row and for its trend alike."""
    rel = {
        "v0.71.0": [
            _c("costas::steps", 1e-3, 4096, {"unit": "sample"}),
            _c("costas::steps", 9e-3, 4096, {"unit": "sample"}),
        ]
    }
    row = _row(_load().page_data(str(_tree(tmp_path, rel))), "c:costas::steps")
    assert row["p"] == [pytest.approx(1e-3 / 4096, rel=1e-3)]


def test_a_unit_change_breaks_a_trend_and_a_bigger_round_does_not(
    tmp_path: Path,
) -> None:
    """From v0.73.0 every C row records its unit (#2373 review): between
    two releases with units, a round that only grew is the same quantity,
    so its since-last change is real, and a changed unit breaks. Across
    into the first set with units, only the round is known on both sides,
    so a round that changed size breaks there. The tree is one the label
    gate accepts: no legacy-units.toml, as after `make bench-labels-fix`."""
    rel = {
        "v0.72.0": [
            _c("costas::steps", 1e-3, 4096),
            _c("costas::get", 1e-6, 1),
            _c("costas::fold", 1e-3, 4096),
        ],
        "v0.73.0": [
            _c("costas::steps", 1e-3, 4096, {"unit": "sample"}),
            _c("costas::get", 1e-6, 1, {"unit": "call"}),
            _c("costas::fold", 2e-3, 8192, {"unit": "sample"}),
        ],
        "v0.74.0": [
            _c("costas::steps", 8e-3, 65536, {"unit": "sample"}),
            _c("costas::get", 1e-6, 1, {"unit": "bin"}),
            _c("costas::fold", 2e-3, 8192, {"unit": "sample"}),
        ],
    }
    ids = ["c:costas::steps", "c:costas::get", "c:costas::fold"]
    tree = _tree(tmp_path, rel, exempt=ids)
    br = _load()
    assert br.label_problems(str(tree)) == []
    data = br.page_data(str(tree))
    steps = _row(data, "c:costas::steps")
    assert steps["brk"] == []
    # values carry four significant digits, so `sl` does too
    assert steps["sl"] == pytest.approx(
        1e-3 / 4096 / (8e-3 / 65536) - 1, rel=1e-3
    )
    get = _row(data, "c:costas::get")
    assert (get["brk"], get["sl"], get["slb"]) == ([2], None, True)
    assert _row(data, "c:costas::fold")["brk"] == [1]


def test_an_unknown_unit_word_is_refused(tmp_path: Path) -> None:
    """jm_bench records any unit word; one the page cannot read would be
    shown per call unannounced, so the gate names it and where to add it."""
    rel = {"v0.73.0": [_c("costas::steps", 1e-3, 4096, {"unit": "widget"})]}
    tree = _tree(tmp_path, rel, exempt=["c:costas::steps"])
    bad = _load().label_problems(str(tree))
    assert any("records unit `widget`" in b for b in bad), bad


def test_a_module_with_no_family_names_the_file(tmp_path: Path) -> None:
    rel = {"v0.73.0": [_c("costas::steps", 1e-3, 4096, {"unit": "sample"})]}
    tree = _tree(tmp_path, rel, exempt=["c:costas::steps"])
    (tree / "just-makeit.toml").write_text(
        MANIFEST + '\n[module.newmod]\nobjects = ["x"]\n', encoding="utf-8"
    )
    bad = _load().label_problems(str(tree))
    assert any(
        "`newmod` is not in MODULE_FAMILY (scripts/bench_report.py)" in b
        for b in bad
    ), bad


def test_the_first_set_with_units_does_not_break_without_legacy_units(
    tmp_path: Path,
) -> None:
    """`make bench-labels-fix` deletes legacy-units.toml at the release that
    first records units, so the old unit is unknown there: the round's size
    decides. Rehearsing v0.73.0 without this broke all 566 C rows."""
    rel = {
        "v0.72.0": [_c("costas::steps", 1e-3, 4096)],
        "v0.73.0": [_c("costas::steps", 1e-3, 4096, {"unit": "sample"})],
    }
    row = _row(_load().page_data(str(_tree(tmp_path, rel))), "c:costas::steps")
    assert (row["brk"], row["slb"]) == ([], False)


def test_writing_the_page_reports_a_dead_label(tmp_path: Path) -> None:
    """`make bench-docs` at a release is where a retired or renamed row's
    label first shows (#2373 review): the writer runs the whole gate, and
    only the depth-1 --check skips the history question."""
    rel = {"v0.71.0": [_c("costas::steps", 1e-3, 4096, {"unit": "sample"})]}
    tree = _tree(tmp_path, rel, LABEL)
    (tree / "docs").mkdir()
    _git(tree, "init", "-q", "-b", "main")
    (tree / "benchmarks" / "labels.toml").write_text(
        LABEL + NEW_LABEL, encoding="utf-8"
    )
    # measured WITH the new row's label, and no such row recorded: dead
    _stamp(tree, "v0.71.0", _commit(tree, "measured with the label"))
    br = _load()
    assert br.cmd_page("docs/benchmarks.md", root=str(tree)) == 1
    assert not (tree / "docs" / "benchmarks.md").exists()


def test_a_label_with_a_count_reads_an_unknown_unit(tmp_path: Path) -> None:
    """A unit word only one bench uses needs no edit to scripts/: a label
    that states the count reads the row, and the gate accepts it."""
    label = (
        '["c:costas::steps"]\nname = "Bytes"\nunit = "byte"\n'
        'definition = "Bytes in."\ndomain = "byte"\ncount = 1\n'
    )
    rel = {"v0.73.0": [_c("costas::steps", 1e-3, 4096, {"unit": "byte"})]}
    br = _load()
    tree = _tree(tmp_path, rel, label)
    assert br.label_problems(str(tree)) == []
    row = _row(br.page_data(str(tree)), "c:costas::steps")
    assert (row["dom"], row["cnt"]) == ("byte", 1)
