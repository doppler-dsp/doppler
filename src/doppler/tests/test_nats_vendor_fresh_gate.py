"""`make nats-vendor-fresh-check` is safe to run, and reads what it claims.

The gate (`scripts/check_nats_vendor_fresh.py`, #1936/#1981) DELETES its
build directory, which made two hazards worth pinning:

- a directory from the environment (`NATS_FRESH_DIR=.`, `=build`) would
  have wiped the checkout or the main tree;
- a recipe that names `$(MAKE)` runs even under `make -n`, so a dry run of
  `make gates` would have executed it.

It also parses doppler's options out of CMakeLists.txt, and a `-D` spelled
in a way the parser does not know must fail rather than drop out of the
check. Its live behaviour (premise reproduced, then the options kept)
needs cmake and a compiler and runs in CI's Build legs; these cases pin the
guards everywhere.
"""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_nats_vendor_fresh.py"

linux_only = pytest.mark.skipif(
    not sys.platform.startswith("linux"),
    reason="the gate skips off Linux before any path check",
)


def _gate(root: Path, build_dir: str, *protect: str) -> str:
    """Run the gate against `root`; it must refuse, and touch nothing."""
    args = [sys.executable, str(SCRIPT), "--root", str(root)]
    args += ["--build-dir", build_dir]
    for p in protect:
        args += ["--protect", p]
    r = subprocess.run(args, capture_output=True, text=True)
    assert r.returncode == 2, r.stdout + r.stderr
    assert "refused, nothing touched" in r.stdout
    return r.stdout


@linux_only
@pytest.mark.parametrize(
    ("build_dir", "why"),
    [
        (".", "not strictly inside"),
        ("..", "not strictly inside"),
        ("/tmp", "not strictly inside"),
        ("build", "protected tree"),
        ("build/sub", "protected tree"),
    ],
    ids=["the-checkout", "its-parent", "outside", "main-tree", "inside-it"],
)
def test_a_dangerous_build_dir_is_refused(
    tmp_path: Path, build_dir: str, why: str
) -> None:
    (tmp_path / "build").mkdir()
    keep = tmp_path / "build" / "precious"
    keep.write_text("x", encoding="utf-8")
    assert why in _gate(tmp_path, build_dir, "build")
    assert keep.read_text(encoding="utf-8") == "x"


@linux_only
def test_an_existing_dir_it_did_not_make_is_refused(tmp_path: Path) -> None:
    other = tmp_path / "somebodys-tree"
    other.mkdir()
    (other / "work").write_text("x", encoding="utf-8")
    assert "was not made by this gate" in _gate(tmp_path, "somebodys-tree")
    assert (other / "work").exists()


@pytest.fixture
def gate(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    monkeypatch.syspath_prepend(str(REPO / "scripts"))
    spec = importlib.util.spec_from_file_location("_t_nvf", SCRIPT)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _args(*lines: str) -> str:
    return "set(_NATS_CONFIGURE_ARGS\n" + "\n".join(lines) + ")\n"


def test_the_real_cmakelists_parses_completely(gate: ModuleType) -> None:
    text = (REPO / "CMakeLists.txt").read_text(encoding="utf-8")
    opts = gate.expected_options(text)
    assert opts["NATS_BUILD_STREAMING"] == "OFF"
    assert "CMAKE_C_COMPILER" not in opts  # the parent's, not expected


@pytest.mark.parametrize(
    "line",
    ["    -DNATS_X:BOOL=OFF", "    -D NATS_X=OFF"],
    ids=["typed", "spaced"],
)
def test_an_unknown_d_spelling_is_refused(gate: ModuleType, line: str) -> None:
    text = _args("    -DCMAKE_C_COMPILER=${CC}", "    -DA=OFF", line)
    with pytest.raises(gate.RefusedError, match="drop out of the check"):
        gate.expected_options(text)


def _dry_run(*args: str, **env: str) -> subprocess.CompletedProcess[str]:
    drop = {"MAKEFLAGS", "MFLAGS", "MAKELEVEL"}
    clean = {k: v for k, v in os.environ.items() if k not in drop} | env
    return subprocess.run(
        ["make", "-n", "--no-print-directory", *args],
        capture_output=True,
        text=True,
        cwd=REPO,
        env=clean,
    )


def test_a_dry_run_executes_nothing() -> None:
    """A recipe naming $(MAKE) runs even under -n; this one must not."""
    r = _dry_run("nats-vendor-fresh-check")
    assert r.returncode == 0, r.stderr
    assert "check_nats_vendor_fresh.py" in r.stdout  # printed...
    assert not (REPO / "build-nats-fresh").exists()  # ...not run
    assert "nats-vendor-fresh:" not in r.stdout + r.stderr


def test_the_environment_cannot_aim_the_delete() -> None:
    """NATS_FRESH_DIR is plain `=`: an exported value is ignored."""
    r = _dry_run("nats-vendor-fresh-check", NATS_FRESH_DIR=".")
    assert r.returncode == 0, r.stderr
    assert "--build-dir build-nats-fresh" in r.stdout
