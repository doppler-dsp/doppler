"""The gate that keeps the general frame primitive free of CCSDS.

``scripts/check_ccsds_isolation.py`` takes files as arguments so this file can
seed tiny ones in ``tmp_path`` rather than asserting against the real
``wfm_frame.{h,c}`` — which would make these tests fail for reasons that have
nothing to do with the gate.

The *scope* is under test as much as the detection. An earlier version scanned
every component and allowlisted the four that include a ``ccsds_tm`` header.
That was broader than the rule the header states, and it was wrong:
``frame -> ccsds_tm -> wfm_frame`` is acyclic and deliberate, and
``objects/frame.toml`` says so. A ratchet over a rule that should never reach
zero pushes toward a refactor nobody wants. So a consumer composing the two is
**not** a finding, and one of these tests pins that.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_ccsds_isolation.py"


def _run(*files: Path):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *[str(f) for f in files]],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def _c(tmp_path: Path, name: str, body: str) -> Path:
    p = tmp_path / name
    p.write_text(body)
    return p


def test_a_clean_primitive_passes(tmp_path: Path) -> None:
    """It composes pn and gold and knows nothing about the standard."""
    f = _c(
        tmp_path,
        "wfm_frame.c",
        '#include "wfm/wfm_frame.h"\n#include "pn/pn_core.h"\n',
    )
    r = _run(f)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "free of ccsds_tm" in r.stdout


def test_an_include_fails(tmp_path: Path) -> None:
    f = _c(tmp_path, "wfm_frame.c", '#include "ccsds_tm/ccsds_tm_frame.h"\n')
    r = _run(f)
    assert r.returncode == 1, r.stdout
    assert "includes a ccsds_tm header" in r.stdout


def test_the_angle_bracket_spelling_is_caught(tmp_path: Path) -> None:
    """One `<>` away from a gate that reports a clean tree."""
    f = _c(tmp_path, "wfm_frame.c", "#include <ccsds_tm/ccsds_tm.h>\n")
    r = _run(f)
    assert r.returncode == 1, r.stdout


def test_a_call_without_an_include_fails(tmp_path: Path) -> None:
    """THE case an include scan misses.

    A forward declaration reaches the kernels just as well as a header does,
    and it is the shape someone reaches for precisely when they know an
    include would be noticed.
    """
    f = _c(
        tmp_path,
        "wfm_frame.c",
        "void ccsds_tm_frame_ops (void *, void *);\n"
        "static void f (void) { ccsds_tm_frame_ops (0, 0); }\n",
    )
    r = _run(f)
    assert r.returncode == 1, r.stdout
    assert "calls ccsds_tm_frame_ops()" in r.stdout


def test_naming_ccsds_in_a_comment_is_not_a_finding(tmp_path: Path) -> None:
    """The header explains the layering BY naming the other side of it.

    A gate that flagged prose is one a reader silences by deleting the
    explanation, which is the opposite of what it is for.
    """
    f = _c(
        tmp_path,
        "wfm_frame.h",
        "/* `ccsds_tm` must depend on this file, so this file must not call\n"
        "   ccsds_tm_frame_ops() or the two form a cycle. */\n"
        "// see ccsds_tm/ccsds_tm_frame.h\n"
        "int wfm_frame_bits (void);\n",
    )
    r = _run(f)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_consumer_composing_both_is_not_scanned(tmp_path: Path) -> None:
    """The scope correction, pinned.

    `frame_core.c` includes `ccsds_tm` on purpose — the acyclic direction the
    design intends. The gate governs the primitive, not every component, so a
    file like this is simply not its business.
    """
    prim = _c(tmp_path, "wfm_frame.c", '#include "pn/pn_core.h"\n')
    _c(tmp_path, "frame_core.c", '#include "ccsds_tm/ccsds_tm_frame.h"\n')
    r = _run(prim)
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_missing_file_is_named(tmp_path: Path) -> None:
    """A gate cannot vouch for a file it did not read.

    Reported per file, so a rename is attributed rather than swallowed by the
    'nothing to read' case below.
    """
    good = _c(tmp_path, "wfm_frame.c", '#include "pn/pn_core.h"\n')
    r = _run(good, tmp_path / "not_here.c")
    assert r.returncode == 1, r.stdout
    assert "did not read" in r.stdout


def test_reading_nothing_at_all_fails(tmp_path: Path) -> None:
    """How a path-scoped check dies quietly: everything is renamed, the scan
    finds nothing, and it reports success forever after."""
    r = _run(tmp_path / "gone.c")
    assert r.returncode == 1, r.stdout
    assert "has not run" in r.stdout


def test_the_real_primitive_is_clean() -> None:
    """The gate as shipped, against `wfm_frame.{h,c}` as they are."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert r.returncode == 0, r.stdout + r.stderr
