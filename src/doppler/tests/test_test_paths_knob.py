"""`make test-python TEST_PATHS=...` collects exactly the paths given.

`TEST_PYTHON_CMD` used to hard-code `pytest src/` and append PYTEST_ARGS,
so a path passed there was collected IN ADDITION to `src/` and the whole
suite ran (#1998). With no way to narrow the target, sessions ran
`pytest <file>` directly, which the make-SSOT hook stops and whose bypass
skips mem-guard and the leak check. TEST_PATHS replaces the hard-coded
`src/`.

These read the command make would run (`make -n`, which executes nothing
here), so a regression that puts `src/` back is caught without running a
suite. The counts that prove the knob end to end (24 tests for one file,
4867 by default, measured 2026-10-09) are in the PR, not here: they move
with the suite.
"""

from __future__ import annotations

import os
import shlex
import subprocess

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)


def _clean_env() -> dict[str, str]:
    """This process's environment without an enclosing make's variables.

    Run under `make test-python TEST_PATHS=...`, make exports its command
    line to children through MAKEFLAGS, and the `make -n` below would
    inherit that TEST_PATHS instead of reading its own default.
    """
    drop = {"MAKEFLAGS", "MFLAGS", "MAKELEVEL", "TEST_PATHS", "PYTEST_ARGS"}
    return {k: v for k, v in os.environ.items() if k not in drop}


def _pytest_line(*make_args: str, **env: str) -> str:
    """The one command line `make test-python` would run that calls pytest.

    ``env`` is added to the cleaned environment, to test what an EXPORTED
    variable does, as opposed to one on make's command line.
    """
    r = subprocess.run(
        ["make", "-n", "--no-print-directory", "test-python", *make_args],
        capture_output=True,
        text=True,
        cwd=REPO,
        env=_clean_env() | env,
    )
    assert r.returncode == 0, r.stderr
    lines = [ln for ln in r.stdout.splitlines() if " pytest " in ln]
    assert len(lines) == 1, r.stdout
    return lines[0]


def _pytest_argv(*make_args: str, **env: str) -> list[str]:
    """The words after `pytest` in the command `make test-python` runs."""
    words = shlex.split(_pytest_line(*make_args, **env))
    return words[words.index("pytest") + 1 :]


def test_the_default_collects_src() -> None:
    argv = _pytest_argv()
    assert argv[0] == "src/"
    assert argv.count("src/") == 1


def test_one_path_replaces_src() -> None:
    path = "src/doppler/spectral/tests/test_psd.py"
    argv = _pytest_argv(f"TEST_PATHS={path}")
    assert argv[0] == path
    assert "src/" not in argv  # narrowed, not added to


def test_several_paths_reach_pytest_as_separate_words() -> None:
    paths = ["src/doppler/agc/tests", "src/doppler/track/tests"]
    argv = _pytest_argv("TEST_PATHS=" + " ".join(paths))
    assert argv[:2] == paths
    assert "src/" not in argv


def test_an_exported_variable_cannot_narrow_the_full_suite() -> None:
    """TEST_PATHS and PYTEST_ARGS are plain `=`: a value left exported in a
    shell is ignored, so `make test-python` stays the full suite."""
    argv = _pytest_argv(
        TEST_PATHS="src/doppler/agc/tests", PYTEST_ARGS="-k nothing"
    )
    assert argv[0] == "src/"
    assert "nothing" not in argv


def test_the_guard_and_the_leak_check_still_wrap_a_narrowed_run() -> None:
    """Narrowing must not drop what made the bypass dangerous."""
    line = _pytest_line("TEST_PATHS=src/doppler/agc/tests")
    assert line.startswith("scripts/mem-guard.sh ")
    assert "check_test_leaks.py --" in line
