"""The instrumented-sweep gate, exercised over seeded makefiles.

`scripts/check_instrumented_sweep.py` guards a defect that blocked every open
PR in this repository for a day. The `validate_*` harnesses register a
`--check` spot check as a ctest entry, cheap in the Release suite and ruinous
under instrumentation: measured on one coverage build over 20 cores
(doppler#1292), the 34 `sweep` validators were 94.3% of the instrumented
ctest CPU, and `validate_acq_surface_jitter` alone took 1261.7 s against a
1266.0 s leg. ASan, UBSan and TSan already excluded them. Coverage did not,
and the omission had no symptom until its 90-minute cap began cancelling the
job -- at which point it read as flaky infrastructure rather than as a leg
running work it did not need.

A gate proven only against a tree that happens to pass is a gate nobody has
seen fail. So the script takes explicit file arguments and this file drives
it over makefiles written here: the real pre-fix shape must be caught, each
way of spelling the exclusion must clear it, and -- because both of these
bit during development -- the two parsing traps must stay fixed.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_instrumented_sweep.py"

# The coverage recipe's shape, reduced to what the gate reads: a define-block
# that configures an instrumented build and then runs ctest.
COVERAGE = """\
COV_DIR ?= build-cov
CTEST := ctest
COV_EXCLUDE_SWEEP = $(if $(COV_SWEEP),,-LE sweep)
define COVERAGE_CMD
$(CMAKE) -B $(COV_DIR) -S . -DDOPPLER_COVERAGE=ON -DBUILD_PYTHON=ON
cd $(COV_DIR) && $(CTEST) --output-on-failure -j 4{flag}
endef
"""

# The sanitizer recipes' shape: the instrumentation marker sits one variable
# away, and a paragraph of prose sits between the build and the ctest.
SANITIZER = """\
TSAN_DIR ?= build-tsan
CTEST := ctest
TSAN_FLAGS = -fsanitize=thread -fno-omit-frame-pointer -g
SAN_EXCLUDE_SWEEP = $(if $(SAN_SWEEP),,-LE sweep)

test-tsan: ## Run the C suite under TSan
\t$(CMAKE) -B $(TSAN_DIR) -S . "-DCMAKE_C_FLAGS=$(TSAN_FLAGS)"
\t$(CMAKE) --build $(TSAN_DIR) --parallel 4
# An empty result set is not a pass, and this comment sits mid-recipe on
# purpose -- make resumes the rule at the next tab-indented line.

\t$(CTEST) --test-dir $(TSAN_DIR){flag} \\
\t\t--output-on-failure
"""


def _check(tmp_path: Path, body: str) -> subprocess.CompletedProcess[str]:
    mk = tmp_path / "Seeded.mk"
    mk.write_text(body, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(mk)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def test_the_real_pre_fix_shape_is_caught(tmp_path: Path) -> None:
    """The coverage leg with no exclusion -- the defect as it shipped."""
    proc = _check(tmp_path, COVERAGE.format(flag=""))
    assert proc.returncode == 1, proc.stdout
    assert "does not exclude the sweep validators" in proc.stdout
    assert "COVERAGE_CMD" in proc.stdout


@pytest.mark.parametrize(
    "flag",
    [" $(COV_EXCLUDE_SWEEP)", " -LE sweep"],
    ids=["through-a-variable", "literal"],
)
def test_either_spelling_of_the_exclusion_clears_it(
    tmp_path: Path, flag: str
) -> None:
    """A variable carrying the flag counts, which is what keeps COV_SWEEP=1."""
    proc = _check(tmp_path, COVERAGE.format(flag=flag))
    assert proc.returncode == 0, proc.stdout


def test_the_marker_is_followed_through_a_variable(tmp_path: Path) -> None:
    """`-fsanitize=` lives in TSAN_FLAGS, not in the recipe text.

    Without expansion the gate recognised only the coverage leg -- one of
    four -- and would have passed this repository on the day the other three
    were the ones already doing it right.
    """
    proc = _check(tmp_path, SANITIZER.format(flag=""))
    assert proc.returncode == 1, proc.stdout
    assert "test-tsan" in proc.stdout


def test_a_mid_recipe_comment_does_not_end_the_rule(tmp_path: Path) -> None:
    """The sanitizer recipes put prose between the build and the ctest.

    Treating a non-indented comment as the end of the rule truncated all
    three sanitizer legs to their configure step, so their ctest lines were
    never examined at all.
    """
    proc = _check(tmp_path, SANITIZER.format(flag=" $(SAN_EXCLUDE_SWEEP)"))
    assert proc.returncode == 0, proc.stdout
    assert "1 instrumented ctest leg" in proc.stdout


def test_a_comment_about_ctest_is_not_a_ctest_call(tmp_path: Path) -> None:
    """These recipes document the call directly above it, in prose."""
    body = COVERAGE.format(flag=" $(COV_EXCLUDE_SWEEP)").replace(
        "define COVERAGE_CMD\n",
        "define COVERAGE_CMD\n# ctest runs it all here, -LE nothing\n",
    )
    proc = _check(tmp_path, body)
    assert proc.returncode == 0, proc.stdout


def test_an_uninstrumented_leg_is_not_policed(tmp_path: Path) -> None:
    """The ordinary Release suite MUST keep running the validators.

    Their `--check` is the gate; excluding it everywhere would delete the
    coverage this trade depends on.
    """
    body = """\
CTEST := ctest
BUILD_DIR ?= build
test: ## Run the default suite
\t$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure
"""
    proc = _check(tmp_path, body)
    # No instrumented leg at all -- the empty-result guard must speak up
    # rather than report a silent pass over nothing.
    assert proc.returncode == 1, proc.stdout
    assert "no instrumented ctest leg" in proc.stdout


def test_the_repository_itself_passes() -> None:
    """The gate over the real makefiles, which is what CI runs."""
    proc = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "instrumented ctest leg(s)" in proc.stdout
