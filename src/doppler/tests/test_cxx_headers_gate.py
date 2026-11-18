"""The C++-includable-headers gate, exercised over seeded include trees.

`scripts/check_cxx_headers.py` exists because no C++ compiler reads a
doppler header anywhere in the build (it is `LANGUAGES C` on purpose), so a
header that could not be included from C++ was reported by a downstream
instead (doppler#3108). Each case seeds an include tree and an exempt list
and points the script at them, because a gate that can only be run against
the real tree cannot be sabotaged.

The compilers are the ones the gate itself defaults to. A box without one
skips here; `make cxx-headers-check` on the same box fails, which is the
behavior the last case pins.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_cxx_headers.py"

#: Parses in both languages: the complex TYPE crosses into C++.
_GOOD = """\
#ifdef __cplusplus
extern "C" {
#endif
float _Complex dp_good_get (const float _Complex *in);
#ifdef __cplusplus
}
#endif
"""

#: The #3108 shape: an inline body that calls the C99 complex library.
_BAD = """\
#include <complex.h>
static inline float dp_bad_re (float _Complex z) { return crealf (z); }
"""

needs_cxx = pytest.mark.skipif(
    not (shutil.which("g++") and shutil.which("clang++")),
    reason="the gate's default compilers, g++ and clang++, are not both here",
)


def _run(
    tmp_path: Path,
    headers: dict[str, str],
    exempt: str = "",
    *extra: str,
    not_cxx: str = "",
) -> subprocess.CompletedProcess[str]:
    """Run the gate over ``headers`` (name -> text) and the two lists."""
    inc = tmp_path / "inc"
    for name, text in headers.items():
        path = inc / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    listing = tmp_path / ".cxx-headers-exempt"
    listing.write_text(exempt, encoding="utf-8")
    second = tmp_path / ".cxx-headers-not-cxx"
    second.write_text(not_cxx, encoding="utf-8")
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--inc",
            str(inc),
            "--exempt",
            str(listing),
            "--not-cxx",
            str(second),
            *extra,
        ],
        capture_output=True,
        text=True,
        check=False,
    )


@needs_cxx
def test_passes_a_tree_that_parses(tmp_path: Path) -> None:
    r = _run(tmp_path, {"doppler/good.h": _GOOD})
    assert r.returncode == 0, r.stdout
    assert "1 of 1 installed headers parse" in r.stdout


@needs_cxx
def test_names_a_header_that_stops_parsing(tmp_path: Path) -> None:
    """The regression itself: a new C99-complex inline body, not exempt."""
    r = _run(tmp_path, {"doppler/good.h": _GOOD, "doppler/bad.h": _BAD})
    assert r.returncode == 1, r.stdout
    assert "doppler/bad.h does not parse as C++" in r.stdout
    assert "crealf" in r.stdout
    assert "good.h" not in r.stdout


@needs_cxx
def test_an_exempt_header_may_fail(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        {"doppler/good.h": _GOOD, "doppler/bad.h": _BAD},
        "doppler/bad.h  # an inline body uses crealf\n",
    )
    assert r.returncode == 0, r.stdout
    assert "1 exempt" in r.stdout


@needs_cxx
def test_the_list_only_shrinks(tmp_path: Path) -> None:
    """A listed header that parses must come off the list."""
    r = _run(
        tmp_path,
        {"doppler/good.h": _GOOD},
        "doppler/good.h  # was broken once\n",
    )
    assert r.returncode == 1, r.stdout
    assert "doppler/good.h parses as C++ now: delete its line" in r.stdout


@needs_cxx
@pytest.mark.parametrize(
    ("line", "fault"),
    [
        ("doppler/bad.h\n", "has no reason"),
        ("doppler/gone.h  # deleted long ago\n", "is not an installed header"),
    ],
)
def test_refuses_a_malformed_list(
    tmp_path: Path, line: str, fault: str
) -> None:
    r = _run(tmp_path, {"doppler/bad.h": _BAD}, line)
    assert r.returncode == 1, r.stdout
    assert fault in r.stdout


@needs_cxx
def test_one_compiler_passing_is_not_enough(tmp_path: Path) -> None:
    """`__builtin_complex` exists in clang++ and not in g++."""
    text = (
        "static inline float _Complex dp_mk (float r)\n"
        "{ return __builtin_complex (r, 0.0f); }\n"
    )
    only_clang = _run(tmp_path, {"doppler/mk.h": text}, "", "--cxx", "clang++")
    assert only_clang.returncode == 0, only_clang.stdout
    both = _run(tmp_path, {"doppler/mk.h": text})
    assert both.returncode == 1, both.stdout
    assert "g++: " in both.stdout


def test_a_missing_compiler_is_not_a_pass(tmp_path: Path) -> None:
    r = _run(
        tmp_path, {"doppler/good.h": _GOOD}, "", "--cxx", "no-such-cxx-xyz"
    )
    assert r.returncode == 1, r.stdout
    assert "has not run, so it has not passed" in r.stdout


def test_an_empty_tree_is_not_a_pass(tmp_path: Path) -> None:
    (tmp_path / "inc").mkdir()
    r = _run(tmp_path, {}, "", "--cxx", sys.executable)
    assert r.returncode == 1, r.stdout
    assert "nothing ran" in r.stdout


# The second list: headers that are not C++ headers at all. What may go on
# it is decided by the gate (private or Python-only, and reached by no
# consumer's include), so each rule gets a case that breaks only it.

#: A private header the tree's own sources share, broken under C++.
_PRIV = {"doppler/x/x_private.h": _BAD}


@needs_cxx
def test_a_private_header_may_be_declared_not_cxx(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        {"doppler/good.h": _GOOD, **_PRIV},
        not_cxx="doppler/x/x_private.h  # internals of x\n",
    )
    assert r.returncode == 0, r.stdout
    assert "1 of 1 installed headers parse" in r.stdout
    assert "1 not C++ headers" in r.stdout


@needs_cxx
def test_a_python_only_header_may_be_declared_not_cxx(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        {"doppler/good.h": _GOOD, "doppler/py.h": "#include <Python.h>\n"},
        not_cxx="doppler/py.h  # a CPython helper\n",
    )
    assert r.returncode == 0, r.stdout


@needs_cxx
def test_a_public_header_cannot_be_declared_not_cxx(tmp_path: Path) -> None:
    """Judgment is not a criterion: neither private nor Python-only."""
    r = _run(
        tmp_path,
        {"doppler/good.h": _GOOD, "doppler/bad.h": _BAD},
        not_cxx="doppler/bad.h  # we would rather not\n",
    )
    assert r.returncode == 1, r.stdout
    assert "`doppler/bad.h` is neither private" in r.stdout


@needs_cxx
@pytest.mark.parametrize(
    "spelling", ['"doppler/x/x_private.h"', '"../x/x_private.h"']
)
def test_a_private_header_a_consumer_reaches_must_parse(
    tmp_path: Path, spelling: str
) -> None:
    """Included by a public header, a private one is installed for C users.

    Both spellings the tree uses resolve: from the include root, and from
    the including header's own directory.
    """
    pub = f"#include {spelling}\n" + _GOOD
    r = _run(
        tmp_path,
        {"doppler/y/y_core.h": pub, **_PRIV},
        "doppler/y/y_core.h  # via x_private.h\n",
        not_cxx="doppler/x/x_private.h  # internals of x\n",
    )
    assert r.returncode == 1, r.stdout
    assert (
        "`doppler/x/x_private.h` is included by doppler/y/y_core.h" in r.stdout
    )


@needs_cxx
def test_a_private_header_another_private_one_reaches_may_stay(
    tmp_path: Path,
) -> None:
    """Only an include from OFF the list makes a header a consumer's."""
    r = _run(
        tmp_path,
        {
            "doppler/good.h": _GOOD,
            "doppler/z/z_private.h": '#include "doppler/x/x_private.h"\n',
            **_PRIV,
        },
        not_cxx=(
            "doppler/x/x_private.h  # internals of x\n"
            "doppler/z/z_private.h  # internals of z\n"
        ),
    )
    assert r.returncode == 0, r.stdout


@needs_cxx
@pytest.mark.parametrize(
    ("exempt", "not_cxx", "fault"),
    [
        (
            "doppler/x/x_private.h  # broken\n",
            "doppler/x/x_private.h  # internals\n",
            "is on both",
        ),
        ("", "doppler/x/x_private.h\n", "has no reason"),
        ("", "doppler/x/gone_private.h  # deleted\n", "not an installed"),
    ],
)
def test_refuses_a_malformed_second_list(
    tmp_path: Path, exempt: str, not_cxx: str, fault: str
) -> None:
    r = _run(
        tmp_path, {"doppler/good.h": _GOOD, **_PRIV}, exempt, not_cxx=not_cxx
    )
    assert r.returncode == 1, r.stdout
    assert fault in r.stdout
