"""The wheel-tag gate, driven over the exact names that shipped (doppler#1817).

v0.60.0 and v0.61.0 published Windows wheels for 3.13/3.14 tagged
``cp313-cpwin_amd64-win_amd64``. PyPI accepted them and pip skipped them,
falling back to the sdist, so a Windows user needed a C toolchain. The gate
(``scripts/check_wheel_tags.py``, behind ``make check-wheel-tags`` in
``release.yml`` and ``make release-wheel-tags`` in ``release-watch``) refuses
a wheel none of whose tags is in ``cpython_tags()`` for its own Python.

This is the sabotage that proves it: the published broken names must go red,
and their corrected spellings -- plus every other platform doppler builds --
must stay green. No network: the ``--pypi``/``--release`` faces only fetch
names, and the judging is what is under test here.
"""

from __future__ import annotations

import subprocess
import sys

import pytest

from doppler.tests._repo import repo_root

SCRIPT = repo_root(__file__) / "scripts" / "check_wheel_tags.py"

# Verbatim from the v0.61.0 PyPI listing.
SHIPPED_BROKEN = [
    "doppler_dsp-0.61.0-cp313-cpwin_amd64-win_amd64.whl",
    "doppler_dsp-0.61.0-cp314-cpwin_amd64-win_amd64.whl",
]

# One per platform doppler builds, from the same listing, plus the corrected
# spellings of the two above.
INSTALLABLE = [
    "doppler_dsp-0.61.0-cp313-cp313-win_amd64.whl",
    "doppler_dsp-0.61.0-cp314-cp314-win_amd64.whl",
    "doppler_dsp-0.61.0-cp39-cp39-win_amd64.whl",
    "doppler_dsp-0.61.0-cp312-cp312-macosx_11_0_arm64.whl",
    "doppler_dsp-0.61.0-cp310-cp310-"
    "manylinux_2_27_x86_64.manylinux_2_28_x86_64.whl",
    "doppler_dsp-0.61.0-cp314-cp314-"
    "manylinux_2_27_aarch64.manylinux_2_28_aarch64.whl",
]


def _gate(*names: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *names],
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.mark.parametrize("name", SHIPPED_BROKEN)
def test_refuses_the_names_that_shipped(name: str) -> None:
    r = _gate(name)
    assert r.returncode == 1, r.stdout
    assert f"FAIL {name}" in r.stdout


def test_accepts_every_platform_doppler_builds() -> None:
    r = _gate(*INSTALLABLE)
    assert r.returncode == 0, r.stdout
    assert f"{len(INSTALLABLE)} installable, 0 refused" in r.stdout


def test_one_bad_wheel_fails_the_whole_set() -> None:
    """A release is refused on one bad name among many good ones."""
    r = _gate(*INSTALLABLE, SHIPPED_BROKEN[0])
    assert r.returncode == 1, r.stdout
    assert "1 refused" in r.stdout


def test_a_path_is_judged_by_its_basename() -> None:
    """release.yml passes dist/*.whl paths, not bare names."""
    assert _gate(f"dist/{INSTALLABLE[0]}").returncode == 0
    assert _gate(f"dist/{SHIPPED_BROKEN[0]}").returncode == 1


@pytest.mark.parametrize(
    ("name", "why"),
    [
        ("doppler_dsp-0.61.0-py3-none-any.whl", "not a CPython"),
        ("doppler_dsp-0.61.0-cp313-cp313.whl", "unparseable"),
        # An unmatched shell glob arrives as its literal pattern.
        ("dist/*.whl", "unparseable"),
    ],
)
def test_refuses_what_is_not_a_cpython_wheel(name: str, why: str) -> None:
    r = _gate(name)
    assert r.returncode == 1, r.stdout
    assert why in r.stdout


def test_nothing_to_check_is_not_a_pass() -> None:
    r = _gate()
    assert r.returncode == 1, r.stdout
    assert "nothing was checked" in r.stdout
