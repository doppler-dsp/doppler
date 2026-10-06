"""Every generated-glue file is recognised as glue by every gate that cares.

jm's CPython glue is `<mod>_ext.c` (the aggregator) and `<mod>_ext_<object>.c`
(one per object). Three gates each carried their own regex for it, all with
`[a-z0-9_]+` for the object part, and three objects keep class-case names:
`RateConverter`, `HalfbandDecimator`, `Resampler`. So those fragments were
treated as hand-written code. It stayed latent until #1446 handed
`RateConverter` to jm and the patch-coverage gate demanded tests for the
defensive guards jm renders: 73 of 101 changed lines "missing" in a file that
is generated (#1848).

The rule now has one Python definition (`_layout.GLUE_FRAGMENT`, used by the
alloc and enum-table gates) and one Makefile copy (`COV_IGNORE`), which cannot
import it. This test holds the two together, and derives the files from the
tree rather than from a list, so a new object named in any case is covered
the day it exists.
"""

from __future__ import annotations

import re
import sys

import pytest

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
sys.path.insert(0, str(REPO / "scripts"))

import _layout  # noqa: E402

#: Class-case fragments that exist today, spelled out so this test stays
#: meaningful if they are ever renamed: the point is the UPPERCASE.
CLASS_CASE = [
    "native/src/resample/resample_ext_RateConverter.c",
    "native/src/resample/resample_ext_HalfbandDecimator.c",
    "native/src/resample/resample_ext_Resampler.c",
]

#: Hand-written siblings that must NOT read as glue.
NOT_GLUE = [
    "native/src/fft/fft_core.c",
    "native/src/x/x_core_ext.h",
    "native/src/x/xext.c",
]


def _cov_ignore() -> re.Pattern[str]:
    """The Makefile's COV_IGNORE, as Python sees it (`$$` is make's `$`)."""
    text = (REPO / "Makefile").read_text(encoding="utf-8")
    m = re.search(r"^COV_IGNORE\s*\?=\s*(.+)$", text, re.M)
    assert m, "COV_IGNORE is no longer defined in the Makefile"
    return re.compile(m.group(1).strip().replace("$$", "$"))


def _glue_in_tree() -> list[str]:
    return sorted(
        p.relative_to(REPO).as_posix()
        for p in (REPO / "native" / "src").rglob("*_ext*.c")
        if p.is_file() and _layout.GLUE_FRAGMENT.search(p.name)
    )


def test_the_tree_has_glue_to_check() -> None:
    """Derived, so it must find something, or every case below is vacuous."""
    assert len(_glue_in_tree()) > 50


@pytest.mark.parametrize("rel", CLASS_CASE)
def test_a_class_case_fragment_is_glue_everywhere(rel: str) -> None:
    name = rel.rsplit("/", 1)[-1]
    assert _layout.GLUE_FRAGMENT.search(name), name
    assert _cov_ignore().search(rel), f"COV_IGNORE misses {rel}"


def test_every_glue_file_in_the_tree_is_ignored_by_coverage() -> None:
    """Generated glue has no tests of its own; the patch gate must not ask."""
    rx = _cov_ignore()
    missed = [f for f in _glue_in_tree() if not rx.search(f)]
    assert not missed, f"COV_IGNORE does not match generated glue: {missed}"


def test_every_ext_file_is_classified_as_glue() -> None:
    """No `*_ext*.c` is both in the tree and outside the shared pattern.

    A name like `foo_ext_Bar-baz.c` that the pattern does not accept would be
    gated as hand-written, which is the failure this exists for.
    """
    odd = [
        p.relative_to(REPO).as_posix()
        for p in (REPO / "native" / "src").rglob("*_ext*.c")
        if p.is_file()
        and not _layout.GLUE_FRAGMENT.search(p.name)
        and not p.name.endswith(("_extra.c", "_ext_extra.c"))
    ]
    assert not odd, (
        f"*_ext*.c files the glue pattern does not recognise: {odd}"
    )


@pytest.mark.parametrize("rel", NOT_GLUE)
def test_hand_written_files_are_not_glue(rel: str) -> None:
    name = rel.rsplit("/", 1)[-1]
    assert not _layout.GLUE_FRAGMENT.search(name), name
    assert not _cov_ignore().search(rel), f"COV_IGNORE swallows {rel}"


def test_the_old_lowercase_pattern_really_missed_them() -> None:
    """Why the fix was needed: the old rule fails on exactly these names."""
    old = re.compile(r"_ext(_[a-z0-9_]+)?\.c$")
    assert all(not old.search(r.rsplit("/", 1)[-1]) for r in CLASS_CASE)
