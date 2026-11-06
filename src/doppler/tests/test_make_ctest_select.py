"""`make test-asan` honours CTEST_SELECT in its count and in its run.

`make test CTEST_SELECT='-R <re>'` narrows the C suite to the matching
ctests. `make test-asan` did not take the knob, so checking one touched
suite under ASan before a push meant the whole 168-test suite (#2911's
pre-push set). It takes it now, in two places that must agree:

- the COUNT (`ctest ... -N`), which refuses an empty set, so a selection
  that matches nothing fails rather than passing over nothing;
- the RUN, which is what the selection is for.

A selection that reached only the run would let a pattern that matches
nothing report a clean pass; one that reached only the count would run the
whole suite under a narrowed count. The case dry-runs the target with a
probe pattern and finds it in both commands.
"""

from __future__ import annotations

import subprocess

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
PROBE = "zz_ctest_select_probe"


def _dry_run() -> str:
    r = subprocess.run(
        [
            "make",
            "-n",
            "-C",
            str(REPO),
            "test-asan",
            f"CTEST_SELECT=-R {PROBE}",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    return r.stdout


def test_test_asan_takes_ctest_select_in_count_and_run() -> None:
    out = _dry_run()
    # Recipe lines continue with a trailing backslash; join them so each
    # command reads as one string.
    cmds = out.replace("\\\n", " ").splitlines()
    ctest = [c for c in cmds if "ctest" in c and "build-asan" in c]
    count = [c for c in ctest if " -N" in c and "Total Tests" in c]
    run = [c for c in ctest if "--output-on-failure" in c]
    assert count, "no test-asan count command found in the dry run"
    assert run, "no test-asan run command found in the dry run"
    assert all(f"-R {PROBE}" in c for c in count), count
    assert all(f"-R {PROBE}" in c for c in run), run
