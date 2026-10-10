"""The doc-targets gate, exercised over seeded pages.

`scripts/check_doc_targets.py` fails when a page names a `make` target that
does not exist. Its first version saw only `make <target>`, so a bare
backticked name passed: CLAUDE.md said "`bench-baseline` / `bench-check`
remain" for targets renamed long before, and the downstream-jm README named
`test-example-tarball` after 6fb7aff51 renamed it (#2116). CLAUDE.md was not
in the page set at all.

A gate proven only against a tree that happens to pass is a gate nobody has
seen fail. So the script takes explicit page arguments and this file drives
it over pages written here: a stale bare name and a stale `make` form must
each be caught; a real target, a declared CI job, a hook and an outside name
must pass; a name outside every target family must be left alone; a
missing page must fail rather than pass vacuously; and the page set must
be every tracked page except the release records.
"""

from __future__ import annotations

import importlib.util
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_doc_targets.py"


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def _page(tmp_path: Path, body: str) -> str:
    # Resolved, because the script reports the resolved path and a tmp
    # directory can sit behind a symlink.
    p = (tmp_path / "page.md").resolve()
    p.write_text(body, encoding="utf-8")
    return str(p)


def test_a_stale_bare_name_is_caught(tmp_path: Path) -> None:
    """The shape CLAUDE.md had: a renamed target, named without `make`."""
    page = _page(
        tmp_path,
        "Benchmarking did not go away, and `bench-save` /\n"
        "`bench-check` remain.\n",
    )
    r = _run(page)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{page}:2: `bench-check` (bare)" in r.stderr
    # The real target beside it is not a finding.
    assert "bench-save" not in r.stderr


def test_a_stale_make_form_is_still_caught(tmp_path: Path) -> None:
    page = _page(tmp_path, "Run `make bench-check` first.\n")
    r = _run(page)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{page}:1: make bench-check" in r.stderr


def test_declared_names_pass(tmp_path: Path) -> None:
    """A target, a CI job, a hook and an outside name all resolve."""
    page = _page(
        tmp_path,
        "`bench-interleaved` measures; `ci-passed` aggregates the jobs;\n"
        "`gen-c-api-drift` is a hook; `nats-server` is the broker.\n",
    )
    r = _run(page)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_name_outside_every_family_is_left_alone(tmp_path: Path) -> None:
    """File stems and flags are not targets; only a target family counts."""
    page = _page(tmp_path, "See `zzq-notes` and `--bench-check`.\n")
    r = _run(page)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_missing_page_fails(tmp_path: Path) -> None:
    r = _run(str(tmp_path / "absent.md"))
    assert r.returncode == 2, r.stdout + r.stderr


def test_the_page_set_is_every_tracked_page_but_the_records() -> None:
    """Derived from git, so CLAUDE.md and deploy/'s READMEs are held (the
    hand list missed both); the changelog names retired targets on
    purpose, so it is not."""
    spec = importlib.util.spec_from_file_location("cdt", SCRIPT)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    pages = mod.pages()
    assert REPO / "CLAUDE.md" in pages
    assert REPO / "deploy" / "docker" / "README.md" in pages
    assert REPO / "CHANGELOG.md" not in pages
    assert not [p for p in pages if "changelog.d" in p.parts]
