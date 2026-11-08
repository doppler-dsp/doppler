"""The dup-include gate, exercised over a seeded tree.

`scripts/check_dup_includes.py` exists because
`burst_capture_core.h` included `correlator_core.h` three times in a row,
and three more headers repeated an include the same way, with nothing to
notice: the include guard makes every copy after the first a no-op, so the
compiler, the tests and the warnings all pass (gh-2752, step 4c).

Each case seeds a fake tree and points `--root` at it, because a gate that
can only run against the real tree cannot be sabotaged.

The cases past the plain repeat guard the gate against being too eager,
and each is load-bearing. The same header in the `#if` and the `#else` of
one conditional is the portable way to choose between two spellings, and
only one branch is ever compiled. An include inside an `#if` repeated after
its `#endif` is needed when the conditional copy may be absent. `"a.h"`
and `<a.h>` search different paths. And an include quoted in a comment is
documentation, not an include.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_dup_includes.py"
HEADER = "native/inc/doppler/a/a_core.h"


def _run(tmp_path: Path, body: str, rel: str = HEADER):
    """Seed one C file at ``rel`` and run the gate over the tree."""
    p = tmp_path / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(body, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


GUARD = "#ifndef A_CORE_H\n#define A_CORE_H\n{}#endif\n"


def test_a_clean_header_passes(tmp_path: Path) -> None:
    body = GUARD.format('#include "b.h"\n#include "c.h"\n')
    r = _run(tmp_path, body)
    assert r.returncode == 0, r.stdout + r.stderr


def test_the_burst_capture_repeat_fails(tmp_path: Path) -> None:
    """The defect as found: one header three times, in a row."""
    inc = '#include "doppler/correlator/correlator_core.h"\n'
    r = _run(tmp_path, GUARD.format('#include "b.h"\n' + inc * 3))
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{HEADER}:5:" in r.stdout
    assert f"{HEADER}:6:" in r.stdout
    assert "first at line 4" in r.stdout


def test_whitespace_does_not_hide_a_repeat(tmp_path: Path) -> None:
    body = GUARD.format('#include "b.h"\n  #  include   "b.h"\n')
    r = _run(tmp_path, body)
    assert r.returncode == 1, r.stdout + r.stderr
    assert f"{HEADER}:4:" in r.stdout


def test_a_repeat_inside_a_later_if_fails(tmp_path: Path) -> None:
    """The first include already holds inside every branch after it."""
    body = GUARD.format('#include "b.h"\n#ifdef X\n#include "b.h"\n#endif\n')
    r = _run(tmp_path, body)
    assert r.returncode == 1, r.stdout + r.stderr


def test_opposite_branches_pass(tmp_path: Path) -> None:
    body = GUARD.format(
        '#if defined(X)\n#include "b.h"\n#elif defined(Y)\n'
        '#include "b.h"\n#else\n#include "b.h"\n#endif\n'
    )
    r = _run(tmp_path, body)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_conditional_copy_then_an_unconditional_one_passes(
    tmp_path: Path,
) -> None:
    body = GUARD.format('#ifdef X\n#include "b.h"\n#endif\n#include "b.h"\n')
    r = _run(tmp_path, body)
    assert r.returncode == 0, r.stdout + r.stderr


def test_quotes_and_angles_are_different_targets(tmp_path: Path) -> None:
    body = GUARD.format('#include "b.h"\n#include <b.h>\n')
    r = _run(tmp_path, body)
    assert r.returncode == 0, r.stdout + r.stderr


def test_an_include_in_a_comment_is_not_one(tmp_path: Path) -> None:
    body = GUARD.format('#include "b.h"\n/*\n#include "b.h"\n*/\n')
    r = _run(tmp_path, body)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_source_file_is_scanned_too(tmp_path: Path) -> None:
    r = _run(tmp_path, "#include <math.h>\n#include <math.h>\n", "x/y.c")
    assert r.returncode == 1, r.stdout + r.stderr
    assert "x/y.c:2:" in r.stdout


def test_vendor_is_not_ours(tmp_path: Path) -> None:
    body = "#include <math.h>\n#include <math.h>\n"
    r = _run(tmp_path, body, "vendor/lib/z.c")
    assert r.returncode == 0, r.stdout + r.stderr
