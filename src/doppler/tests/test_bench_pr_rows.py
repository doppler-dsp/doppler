"""The pull request half of the bench label gate (doppler#2390).

`bench_report.py --check-labels` holds the rows the newest RELEASE
measured, so a row a pull request adds is invisible to it until the
release after it merges. v0.83.0 is the case: #3029 added five
HalfbandDecimatorR2C rows with no label, and they surfaced only at
`make bench-docs` on release day (labelled late, in #3035).

The pull request half reads the rows the PR's own CI run measured -- C rows
from ``check_bench_runs.py --rows``, Python rows from the
``doppler.tests._bench_rows`` pytest plugin -- and holds them with
``bench_report.py --check-pr-rows``. Every case here runs the REAL
producer into the REAL checker, so a broken producer fails here too:

- an unlabelled C row (the #3029 replay) is refused, and labelling it
  passes;
- an unlabelled Python row is refused, including a row a bench renames
  itself, across xdist workers;
- a C row whose recorded unit contradicts its label is refused, and so is a
  unit word the page cannot read;
- an empty rows file is refused, not passed;
- the plugin writes nothing unless asked;
- a Python with no TOML parser fails the checker loudly, never skips it.
"""

from __future__ import annotations

import json
import os
import stat
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root
from doppler.tests.test_bench_page import _load

if TYPE_CHECKING:
    from pathlib import Path

SCRIPTS = repo_root() / "scripts"

#: The label #3035 gave one of #3029's rows, trimmed to what the gate reads.
R2C = "c:HalfbandDecimatorR2C::execute[block=1024]"
R2C_LABEL = f"""\
["{R2C}"]
name = "Half-band decimator, real in, complex out, 1,024-sample blocks"
unit = "sample"
definition = "One block of 1,024 real float samples decimated by two."
samples = 1
"""


def _fake_bench(build: Path, comp: str, rows: list) -> None:
    """A ``bench_<comp>_core`` that writes jm_bench's JSON for ``rows``.

    ``rows`` is ``[(row name, unit)]``. jm_bench writes the bare row name
    and the unit under ``extra_info``, into the working directory, and
    exits 0; this does exactly that, so `check_bench_runs.py` reads it the
    way it reads a real one.
    """
    data = {
        "benchmarks": [
            {
                "name": name,
                "fullname": f"bench_{comp}_core::{name}",
                "extra_info": {"unit": unit},
                "stats": {"mean": 1e-6, "iterations": 1},
            }
            for name, unit in rows
        ]
    }
    exe = build / f"bench_{comp}_core"
    exe.write_text(
        "#!/bin/sh\n"
        f"cat > bench_{comp}_core.json <<'JSON'\n{json.dumps(data)}\nJSON\n",
        encoding="utf-8",
    )
    exe.chmod(exe.stat().st_mode | stat.S_IXUSR)


def _c_rows(tmp_path: Path, rows: list, comp: str = "HalfbandDecimatorR2C"):
    """Run the real `check_bench_runs.py --rows` over one fake bench."""
    build = tmp_path / "build"
    build.mkdir()
    _fake_bench(build, comp, rows)
    out = tmp_path / "c-rows.json"
    r = subprocess.run(
        [
            sys.executable,
            str(SCRIPTS / "check_bench_runs.py"),
            str(build),
            "--rows",
            str(out),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    return out


def _tables(root: Path, labels: str = "", exempt=()) -> Path:
    (root / "benchmarks").mkdir(parents=True, exist_ok=True)
    (root / "benchmarks" / "labels.toml").write_text(labels, "utf-8")
    (root / "benchmarks" / ".labels-exempt").write_text(
        "".join(f"{e}\n" for e in exempt), "utf-8"
    )
    return root


def _gate(root: Path, *rows_files: Path) -> list:
    bad, _ = _load().pr_row_problems(str(root), [str(f) for f in rows_files])
    return bad


def test_an_unlabelled_c_row_is_refused_as_3029s_were(tmp_path: Path) -> None:
    rows = _c_rows(tmp_path, [("execute[block=1024]", "sample")])
    bad = _gate(_tables(tmp_path / "tree"), rows)
    assert any(f"`{R2C}` has no label" in b for b in bad), bad
    assert _gate(_tables(tmp_path / "ok", R2C_LABEL), rows) == []


def test_a_new_row_cannot_hide_behind_an_old_exempt_line(
    tmp_path: Path,
) -> None:
    """Only the exact row is exempt: a sibling the PR adds is not."""
    rows = _c_rows(
        tmp_path,
        [("execute[block=1024]", "sample"), ("execute[block=8]", "sample")],
    )
    bad = _gate(_tables(tmp_path / "tree", exempt=[R2C]), rows)
    assert bad == [
        "`c:HalfbandDecimatorR2C::execute[block=8]` has no label: this pull "
        "request measures it, so label it here -- a name, unit, definition "
        "and count (`samples`, or `domain` and `count`) in "
        "benchmarks/labels.toml"
    ]


def test_a_c_unit_that_contradicts_its_label_is_refused(
    tmp_path: Path,
) -> None:
    rows = _c_rows(tmp_path, [("execute[block=1024]", "bin")])
    bad = _gate(_tables(tmp_path / "tree", R2C_LABEL), rows)
    assert any(
        "labelled unit `sample` but the snapshot records `bin`" in b
        for b in bad
    ), bad


def test_a_unit_word_the_page_cannot_read_is_refused(tmp_path: Path) -> None:
    rows = _c_rows(tmp_path, [("execute[block=1024]", "widget")])
    bad = _gate(_tables(tmp_path / "tree", exempt=[R2C]), rows)
    assert any("records unit `widget`, which the page" in b for b in bad), bad


def test_an_empty_rows_file_is_refused_not_passed(tmp_path: Path) -> None:
    empty = tmp_path / "rows.json"
    empty.write_text(json.dumps({"suite": "c", "benches": []}), "utf-8")
    bad = _gate(_tables(tmp_path / "tree"), empty)
    assert any("a check of nothing is not a pass" in b for b in bad), bad


BENCH_PY = """\
import pytest


def test_bench_steps_64k(benchmark):
    benchmark(sum, range(8))


@pytest.mark.parametrize("R", [4, 8])
def test_bench_params(benchmark, R):
    benchmark.name = f"decimate_R{R}"
    benchmark(sum, range(R))
"""


def _py_rows(
    tmp_path: Path, *extra: str, bench: str = BENCH_PY, rc: int = 0
) -> Path:
    """Run the real plugin over one bench file, as `make test-python` does."""
    proj = tmp_path / "proj"
    bdir = proj / "src" / "doppler" / "track" / "benchmarks"
    bdir.mkdir(parents=True)
    (bdir / "bench_costas.py").write_text(bench, "utf-8")
    out = tmp_path / "py-rows.json"
    env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1"}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "-p",
            "doppler.tests._bench_rows",
            "-p",
            "no:cacheprovider",
            "--benchmark-disable",
            "-q",
            "--rootdir",
            str(proj),
            "-c",
            os.devnull,
            *extra,
            "src/doppler/track/benchmarks/bench_costas.py",
        ],
        cwd=proj,
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    assert r.returncode == rc, r.stdout + r.stderr
    return out


ROWS_PY = (
    "py:costas::decimate_R4",
    "py:costas::decimate_R8",
    "py:costas::test_bench_steps_64k",
)


@pytest.mark.parametrize("workers", [[], ["-n", "2"]], ids=["serial", "xdist"])
def test_an_unlabelled_python_row_is_refused(
    tmp_path: Path, workers: list
) -> None:
    """Every row, its self-renamed ones included, reaches the gate."""
    out = tmp_path / "py-rows.json"
    _py_rows(tmp_path, f"--bench-rows={out}", *workers)
    bad = _gate(_tables(tmp_path / "tree"), out)
    for rid in ROWS_PY:
        assert any(f"`{rid}` has no label" in b for b in bad), bad
    assert len(bad) == len(ROWS_PY), bad
    assert _gate(_tables(tmp_path / "ok", exempt=ROWS_PY), out) == []


def test_the_plugin_writes_nothing_unless_asked(tmp_path: Path) -> None:
    _py_rows(tmp_path)
    assert not list(tmp_path.rglob("*rows*.json"))


def test_a_python_with_no_toml_parser_fails_loudly(tmp_path: Path) -> None:
    """The C leg runs on the image's system Python: if a future image has
    no tomllib, the gate must go red with the reason, never skip."""
    rows = _c_rows(tmp_path, [("execute[block=1024]", "sample")])
    code = (
        "import runpy, sys\n"
        "sys.modules['tomllib'] = None\n"
        "sys.modules['tomli'] = None\n"
        f"sys.argv = ['bench_report.py', '--check-pr-rows', {str(rows)!r}]\n"
        f"runpy.run_path({str(SCRIPTS / 'bench_report.py')!r}, "
        "run_name='__main__')\n"
    )
    r = subprocess.run(
        [sys.executable, "-c", code],
        cwd=_tables(tmp_path / "tree", R2C_LABEL),
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode != 0
    assert "tomli" in r.stderr, r.stderr


def _cli(root: Path, rows: Path) -> subprocess.CompletedProcess:
    """`bench_report.py --check-pr-rows` as CI runs it, by its exit code."""
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPTS / "bench_report.py"),
            "--check-pr-rows",
            str(rows),
        ],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    )


def test_the_gate_exits_nonzero_on_a_refusal_and_zero_on_a_pass(
    tmp_path: Path,
) -> None:
    """CI reads the exit code, not the messages: hold both."""
    rows = _c_rows(tmp_path, [("execute[block=1024]", "sample")])
    red = _cli(_tables(tmp_path / "red"), rows)
    assert red.returncode == 1, red.stdout + red.stderr
    assert f"`{R2C}` has no label" in red.stdout
    green = _cli(_tables(tmp_path / "green", R2C_LABEL), rows)
    assert green.returncode == 0, green.stdout + green.stderr
    assert "all 1 row(s) this run measured" in green.stdout
    gone = _cli(_tables(tmp_path / "gone"), tmp_path / "missing.json")
    assert gone.returncode != 0
    assert "no rows file" in gone.stderr


def test_a_bench_that_exits_0_and_records_nothing_fails(
    tmp_path: Path,
) -> None:
    """Its rows would reach no gate, so bench-run-check refuses it, and
    leaves no rows file behind: not even last run's."""
    build = tmp_path / "build"
    build.mkdir()
    exe = build / "bench_silent_core"
    exe.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    exe.chmod(exe.stat().st_mode | stat.S_IXUSR)
    out = tmp_path / "c-rows.json"
    out.write_text('{"suite": "c", "benches": [{"stale": 1}]}', "utf-8")
    r = subprocess.run(
        [
            sys.executable,
            str(SCRIPTS / "check_bench_runs.py"),
            str(build),
            "--rows",
            str(out),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 1
    assert "exit 0, but wrote no bench_silent_core.json" in r.stderr
    assert not out.exists()


def test_a_run_that_dies_leaves_no_stale_rows_file(tmp_path: Path) -> None:
    """A run killed before its session ends writes no rows, and must not
    leave last run's file for the gate to read as this run's."""
    out = tmp_path / "py-rows.json"
    out.write_text('{"suite": "python", "benches": [{"stale": 1}]}', "utf-8")
    dies = "import os\n\n\ndef test_bench_dies(benchmark):\n    os._exit(3)\n"
    _py_rows(tmp_path, f"--bench-rows={out}", bench=dies, rc=3)
    assert not out.exists()
