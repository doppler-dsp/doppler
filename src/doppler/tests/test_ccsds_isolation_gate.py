"""The CCSDS-isolation gate, driven over seeded trees.

``scripts/check_ccsds_isolation.py`` takes roots as arguments so this file can
build a handful of `.c` files in ``tmp_path`` rather than asserting against the
real ``native/`` tree — which would make every test here fail the day a site is
legitimately cut, exactly when the gate is working.

The distinction under test is that this is a **ratchet, not an allowlist**. An
allowlist fails in one direction: something new appeared. That kind of list
rots into a permanent exemption, because nothing notices when an entry stops
being true. So the gate fails in both directions, and the second one — a name
on the list that no longer reaches into ``ccsds_tm`` — is what these tests care
about most.
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

INCLUDE = '#include "ccsds_tm/ccsds_tm_frame.h"\n'


def _tree(tmp_path: Path, files: dict[str, str]) -> Path:
    """Materialise ``{relative path: contents}`` under a ``native/`` root."""
    for rel, body in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body)
    return tmp_path / "native"


def _run(root: Path, allowed: set[str] | None = None):
    """Run the gate over *root*, optionally overriding the allowlist.

    The override is what lets these tests seed a *specific* ratchet state
    without editing the shipped list.
    """
    script = SCRIPT.read_text()
    if allowed is not None:
        # `set(...)` explicitly: `{}` is an empty dict, and a gate seeded with
        # one fails on `set - dict` rather than on what the test is about.
        body = ", ".join(f'"{p}"' for p in sorted(allowed))
        start = script.index("ALLOWED = {")
        end = script.index("}", start) + 1
        script = script[:start] + f"ALLOWED = set([{body}])" + script[end:]
    tmp = root.parent.parent / "_gate.py"
    tmp.write_text(script)
    # cwd is the tree's parent, so reported paths read `native/src/...` exactly
    # as they do in the repo.
    return subprocess.run(
        [sys.executable, str(tmp), "native/src"],
        capture_output=True,
        text=True,
        cwd=root.parent,
    )


def test_a_clean_tree_passes(tmp_path: Path) -> None:
    """Nothing outside the component reaches in."""
    root = _tree(
        tmp_path,
        {
            "native/src/frame/frame_core.c": '#include "wfm/wfm_frame.h"\n',
            "native/src/ccsds_tm/desc.c": INCLUDE,
        },
    )
    r = _run(root, allowed=set())
    assert r.returncode == 0, r.stdout + r.stderr
    assert "none new" in r.stdout


def test_the_component_itself_is_not_a_violation(tmp_path: Path) -> None:
    """`ccsds_tm` including its own headers is the whole point of it."""
    root = _tree(tmp_path, {"native/src/ccsds_tm/frame.c": INCLUDE})
    r = _run(root, allowed=set())
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_new_violator_fails(tmp_path: Path) -> None:
    """The breakage grew — the direction any allowlist catches."""
    root = _tree(tmp_path, {"native/src/wfm/wfm_new.c": INCLUDE})
    r = _run(root, allowed=set())
    assert r.returncode == 1, r.stdout
    assert "not on the list" in r.stdout
    assert "native/src/wfm/wfm_new.c" in r.stdout


def test_a_stale_allowlist_entry_fails(tmp_path: Path) -> None:
    """THE case a plain allowlist misses.

    The site was cut and the list was not. Without this direction the entry
    survives as a permanent exemption, and the next file to take that path
    inherits it silently.
    """
    root = _tree(
        tmp_path,
        {"native/src/frame/frame_core.c": '#include "wfm/wfm_frame.h"\n'},
    )
    r = _run(root, allowed={"native/src/frame/frame_core.c"})
    assert r.returncode == 1, r.stdout
    assert "no longer reach" in r.stdout
    assert "may only shrink" in r.stdout


def test_a_known_site_still_violating_passes(tmp_path: Path) -> None:
    """The ratchet holds where it is; it does not demand the cut today."""
    root = _tree(tmp_path, {"native/src/frame/frame_core.c": INCLUDE})
    r = _run(root, allowed={"native/src/frame/frame_core.c"})
    assert r.returncode == 0, r.stdout + r.stderr


def test_the_angle_bracket_spelling_is_caught(tmp_path: Path) -> None:
    """One `<>` away from a gate that reports a clean tree."""
    root = _tree(
        tmp_path,
        {"native/src/wfm/w.c": "#include <ccsds_tm/ccsds_tm.h>\n"},
    )
    r = _run(root, allowed=set())
    assert r.returncode == 1, r.stdout
    assert "native/src/wfm/w.c" in r.stdout


def test_scanning_nothing_is_a_failure(tmp_path: Path) -> None:
    """A scan that matches nothing has not passed — it has not run.

    This is how a path-scoped gate dies quietly: a directory is renamed, the
    scan finds no files, and reports success forever after.
    """
    root = _tree(tmp_path, {"native/src/.keep": ""})
    r = _run(root, allowed=set())
    assert r.returncode == 1, r.stdout
    assert "has not run" in r.stdout


def test_an_absent_root_is_a_failure(tmp_path: Path) -> None:
    """Same failure, one step earlier."""
    (tmp_path / "native").mkdir()
    r = _run(tmp_path / "native", allowed=set())
    assert r.returncode == 1, r.stdout
    assert "has not passed" in r.stdout


def test_the_shipped_allowlist_matches_the_real_tree() -> None:
    """The gate as shipped, against `native/` as it is.

    Not a duplicate of `make lint`: it is what makes a stale entry fail here,
    in a targeted run, rather than only in the full lint.
    """
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )
    assert r.returncode == 0, r.stdout + r.stderr
