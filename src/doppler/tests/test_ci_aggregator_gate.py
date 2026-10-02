"""The `CI passed` aggregator gate, exercised over seeded workflows.

`scripts/check_ci_aggregator.py` exists because `protect-main` was relaxed on
2026-09-14 to require a single status check, `CI passed`. From then on a job
gates a merge only through that job's `needs` list -- a hand-kept YAML list
that nothing read. Each case below seeds the shape that would make a red job
stop blocking, so it fails if the gate cannot see it.
"""

from __future__ import annotations

import subprocess
import sys
import textwrap
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_ci_aggregator.py"


def _check(tmp_path: Path, body: str):
    f = tmp_path / "ci.yml"
    f.write_text(textwrap.dedent(body), encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(f)], capture_output=True, text=True
    )


_SOUND = """
    jobs:
      build:
        runs-on: ubuntu-latest
      test:
        runs-on: ubuntu-latest
      ci-passed:
        name: CI passed
        needs: [build, test]
        if: always()
        runs-on: ubuntu-latest
"""


def test_a_sound_aggregator_passes(tmp_path: Path) -> None:
    r = _check(tmp_path, _SOUND)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "all 2 job(s)" in r.stdout


def test_a_job_missing_from_needs_is_named(tmp_path: Path) -> None:
    """The failure this gate exists for: a new job nobody added to needs."""
    r = _check(
        tmp_path,
        _SOUND.replace(
            "      ci-passed:",
            "      lint:\n        runs-on: ubuntu-latest\n      ci-passed:",
        ),
    )
    assert r.returncode == 1
    assert "`lint` is not in `ci-passed`'s needs" in r.stdout


def test_a_need_that_names_no_job_is_named(tmp_path: Path) -> None:
    r = _check(
        tmp_path, _SOUND.replace("[build, test]", "[build, test, gone]")
    )
    assert r.returncode == 1
    assert "needs `gone`, which is not a job here" in r.stdout


def test_a_renamed_aggregator_is_refused(tmp_path: Path) -> None:
    """The ruleset requires the check by its display NAME."""
    r = _check(tmp_path, _SOUND.replace("name: CI passed", "name: CI green"))
    assert r.returncode == 1
    assert "requires the check 'CI passed'" in r.stdout


def test_an_aggregator_without_always_is_refused(tmp_path: Path) -> None:
    """Without `if: always()` a failed need SKIPS it; a skip does not block."""
    r = _check(tmp_path, _SOUND.replace("        if: always()\n", ""))
    assert r.returncode == 1
    assert "must run `if: always()`" in r.stdout


def test_no_aggregator_is_refused(tmp_path: Path) -> None:
    body = _SOUND.split("      ci-passed:")[0]
    r = _check(tmp_path, body)
    assert r.returncode == 1
    assert "no `ci-passed` job" in r.stdout


def test_the_live_ci_workflow_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True, cwd=REPO
    )
    assert r.returncode == 0, r.stdout + r.stderr


_FAST = """
    jobs:
      changes:
        runs-on: ubuntu-latest
        outputs:
          src: x
      lint:
        runs-on: ubuntu-latest
      matrix:
        needs: {needs}
        if: needs.changes.outputs.src == 'true'
        runs-on: ubuntu-latest
      ci-passed:
        name: CI passed
        needs: [changes, lint, matrix]
        if: always()
        runs-on: ubuntu-latest
        steps:
          - env:
              SKIPPABLE: {skippable}
            run: python3 scripts/ci_passed.py
"""


def test_a_sound_fast_path_passes(tmp_path: Path) -> None:
    r = _check(tmp_path, _FAST.format(needs="changes", skippable="matrix"))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_gated_job_must_need_changes(tmp_path: Path) -> None:
    # Without it the condition reads an empty output: skipped on every diff.
    r = _check(tmp_path, _FAST.format(needs="lint", skippable="matrix"))
    assert r.returncode == 1
    assert "does not need it" in r.stdout


def test_a_gated_job_missing_from_skippable_is_caught(
    tmp_path: Path,
) -> None:
    r = _check(tmp_path, _FAST.format(needs="changes", skippable="other"))
    assert r.returncode == 1
    assert "not in `ci-passed`'s SKIPPABLE" in r.stdout


def test_a_gated_output_changes_does_not_declare_is_refused(
    tmp_path: Path,
) -> None:
    body = _FAST.format(needs="changes", skippable="matrix").replace(
        "        outputs:\n          src: x\n", ""
    )
    r = _check(tmp_path, body)
    assert r.returncode == 1
    assert "declares no `src` output" in r.stdout


# The docs-only permission, held the same way: CODE_ONLY == jobs on `code`.
_CODE = """
    jobs:
      changes:
        runs-on: ubuntu-latest
        outputs:
          src: x
          code: x
      lint:
        runs-on: ubuntu-latest
      macos:
        needs: changes
        if: needs.changes.outputs.code == 'true'
        runs-on: ubuntu-latest
      ci-passed:
        name: CI passed
        needs: [changes, lint, macos]
        if: always()
        runs-on: ubuntu-latest
        steps:
          - env:
              CODE_ONLY: {code_only}
            run: python3 scripts/ci_passed.py
"""


def test_a_sound_docs_lane_passes(tmp_path: Path) -> None:
    r = _check(tmp_path, _CODE.format(code_only="macos"))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_code_job_missing_from_code_only_is_caught(tmp_path: Path) -> None:
    r = _check(tmp_path, _CODE.format(code_only="other"))
    assert r.returncode == 1
    assert "not in `ci-passed`'s CODE_ONLY" in r.stdout


def test_code_only_may_not_name_an_ungated_job(tmp_path: Path) -> None:
    # The dangerous direction: a docs-only PR could skip lint, green.
    r = _check(tmp_path, _CODE.format(code_only="macos lint"))
    assert r.returncode == 1
    assert "CODE_ONLY lists `lint`, which is not gated" in r.stdout


def test_skippable_may_not_name_an_ungated_job(tmp_path: Path) -> None:
    # The dangerous direction: permission to skip a gate that never skips.
    r = _check(
        tmp_path, _FAST.format(needs="changes", skippable="matrix lint")
    )
    assert r.returncode == 1
    assert "lists `lint`, which is not gated" in r.stdout


# The merge queue's pull_request split, retired 2026-10-01: every run is full,
# so each trace of it is a skip permission nothing takes. Each case below
# restores ONE trace onto an otherwise sound workflow.
_RETIRED = """
    jobs:
      changes:
        runs-on: ubuntu-latest
        outputs:
          src: x
          EXTRA_OUTPUT
      lint:
        runs-on: ubuntu-latest
      matrix:
        needs: changes
        if: GATE
        runs-on: ubuntu-latest
      ci-passed:
        name: CI passed
        needs: [changes, lint, matrix]
        if: always()
        runs-on: ubuntu-latest
        steps:
          - env:
              SKIPPABLE: matrix
              EXTRA_ENV
            run: python3 scripts/ci_passed.py
"""


def _retired(
    output: str = "docs: x",
    gate: str = "needs.changes.outputs.src == 'true'",
    env: str = "NEEDS: x",
) -> str:
    return (
        _RETIRED.replace("EXTRA_OUTPUT", output)
        .replace("GATE", gate)
        .replace("EXTRA_ENV", env)
    )


def test_no_split_passes(tmp_path: Path) -> None:
    r = _check(tmp_path, _retired())
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.parametrize("key", ["full", "heavy", "primary_full"])
def test_a_retired_output_is_refused(tmp_path: Path, key: str) -> None:
    r = _check(tmp_path, _retired(output=f"{key}: x"))
    assert r.returncode == 1
    assert f"`changes` declares `{key}`" in r.stdout


def test_a_job_gated_on_heavy_is_refused(tmp_path: Path) -> None:
    r = _check(tmp_path, _retired(gate="needs.changes.outputs.heavy == 1"))
    assert r.returncode == 1
    assert "job `matrix` is gated on a retired" in r.stdout


def test_a_heavy_list_is_refused(tmp_path: Path) -> None:
    r = _check(tmp_path, _retired(env="HEAVY: matrix"))
    assert r.returncode == 1
    assert "declares HEAVY" in r.stdout


# A python job whose single-leg steps name their leg by role (doppler#1714).
# The fence step's selector, `changes`' outputs and how it derives them are
# the seams each case below breaks.
_LEGS = """
    jobs:
      changes:
        runs-on: ubuntu-latest
        outputs:
          OUTPUTS
        steps:
          - id: classify
            run: DERIVE
      python:
        needs: changes
        runs-on: ubuntu-latest
        steps:
          - name: Fences
            if: SELECTOR
            run: make test-snippets
          - name: Test
            if: matrix.python-version != needs.changes.outputs.primary
            run: make test-python
          - name: Test with coverage
            if: >-
              ${{ matrix.python-version ==
                  needs.changes.outputs.primary }}
            run: make test-python PYTEST_ARGS=--cov
      ci-passed:
        name: CI passed
        needs: [changes, python]
        if: always()
        runs-on: ubuntu-latest
"""
_FAST_SEL = "matrix.python-version == needs.changes.outputs.primary"


def _legs(
    sel: str = _FAST_SEL,
    outputs: str = "primary: x",
    derive: str = "p=$(python3 scripts/python_versions.py --primary)",
) -> str:
    return (
        _LEGS.replace("SELECTOR", sel)
        .replace("OUTPUTS", outputs)
        .replace("DERIVE", derive)
    )


def test_steps_named_by_role_pass(tmp_path: Path) -> None:
    r = _check(tmp_path, _legs())
    assert r.returncode == 0, r.stdout + r.stderr
    assert "3 single-leg step(s), named by role" in r.stdout
    assert "primary  make test-snippets" in r.stdout
    assert "rest     make test-python" in r.stdout


def test_a_version_literal_selector_is_refused(tmp_path: Path) -> None:
    """The failure #1714 names: a leg that vanishes with its classifier."""
    r = _check(tmp_path, _legs(sel="matrix.python-version == '3.12'"))
    assert r.returncode == 1
    assert "step `Fences` picks a Python leg" in r.stdout
    assert "a version literal" in r.stdout


def test_a_negated_version_literal_is_refused(tmp_path: Path) -> None:
    r = _check(tmp_path, _legs(sel="matrix.python-version != '3.12'"))
    assert r.returncode == 1
    assert "a version literal" in r.stdout


def test_an_undeclared_selector_is_refused(tmp_path: Path) -> None:
    # Named by role, but in no lane LEG_SELECTORS declares.
    sel = _FAST_SEL + " && github.event_name == 'push'"
    r = _check(tmp_path, _legs(sel=sel))
    assert r.returncode == 1
    assert "an undeclared selector" in r.stdout


def test_a_missing_primary_output_is_refused(tmp_path: Path) -> None:
    r = _check(tmp_path, _legs(outputs="docs: x"))
    assert r.returncode == 1
    assert "declares no `primary` output" in r.stdout


def test_a_primary_written_down_is_refused(tmp_path: Path) -> None:
    # The literal moved out of the selector and into `changes`.
    r = _check(tmp_path, _legs(derive="echo primary=3.9"))
    assert r.returncode == 1
    assert "does not derive the primary leg" in r.stdout


def test_a_selecting_job_must_need_changes(tmp_path: Path) -> None:
    body = _legs().replace("        needs: changes\n", "", 1)
    r = _check(tmp_path, body)
    assert r.returncode == 1
    assert "does not need `changes`" in r.stdout


_DOC = """\
# CI

<!-- python-legs:start -->

| step | target | lane | cost |
| --- | --- | --- | --- |
| Fences | `make test-snippets` | FENCE_LANE | 1 s |
| Test | `make test-python` | rest | 1 s |
| Test with coverage | `make test-python` | primary | 1 s |
EXTRA
<!-- python-legs:end -->
"""


def _check_doc(tmp_path: Path, doc: str):
    f = tmp_path / "ci.yml"
    f.write_text(textwrap.dedent(_legs()), encoding="utf-8")
    d = tmp_path / "ci.md"
    d.write_text(doc, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(f), "--doc", str(d)],
        capture_output=True,
        text=True,
    )


def _doc(lane: str = "primary", extra: str = "") -> str:
    return _DOC.replace("FENCE_LANE", lane).replace("EXTRA", extra)


def test_a_doc_table_that_matches_passes(tmp_path: Path) -> None:
    r = _check_doc(tmp_path, _doc())
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_doc_table_with_the_wrong_lane_is_refused(tmp_path: Path) -> None:
    r = _check_doc(tmp_path, _doc(lane="rest"))
    assert r.returncode == 1
    assert "lacks `Fences` | `make test-snippets` | primary" in r.stdout
    assert "states `Fences` | `make test-snippets` | rest" in r.stdout


def test_a_doc_table_with_a_ghost_row_is_refused(tmp_path: Path) -> None:
    r = _check_doc(
        tmp_path, _doc(extra="| Gone | `make gone` | primary | 1 |")
    )
    assert r.returncode == 1
    assert "states `Gone`" in r.stdout


def test_a_doc_without_the_table_is_refused(tmp_path: Path) -> None:
    r = _check_doc(tmp_path, "# CI\n")
    assert r.returncode == 1
    assert "python-legs:start" in r.stdout
