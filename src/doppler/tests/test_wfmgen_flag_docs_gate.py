"""The wfmgen flag-documentation gate, exercised over a seeded tree.

`scripts/check_wfmgen_flag_docs.py` exists because the property it enforces
was established once, by hand, and kept by nothing. A review counted 50 of
wfmgen's 56 flags reachable from the guide; the missing six were the whole
channel-coding stage. doppler#1044 wrote that page. Nothing then stopped the
57th flag from arriving in silence, which is exactly how the six arrived.

Each case seeds a fake tree, because a gate that can only be tested against
the real one cannot be sabotaged: you would have to leave doppler's own docs
broken to check it, and nobody does that twice.

The cases that matter are the ones where a WEAKER gate still passes. A
substring test would call `--interleave` documented on the strength of
`--interleave-unit`; a name-only test would never notice an alias; and a gate
that globs an empty directory reports success having read nothing. All three
are pinned below, because all three are how this check could look green while
the guide had a hole in it.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_wfmgen_flag_docs.py"

# dispatcher_flags() refuses to trust an option table it cannot recognise, so
# every fixture carries the anchors it looks for. That coupling is deliberate:
# a fixture that could omit them would also be a fixture that kept passing
# after the real table's format changed.
ANCHORS = ["--type", "--count", "--output", "--freq"]


def _row(name: str, alias: str | None = None) -> str:
    row = f'  {{ .name  = "{name}",\n'
    if alias:
        row += f'    .alias = "{alias}",\n'
    return row + "    .off   = 0 },\n"


def _seed(tmp_path: Path, flags: list[str], pages: dict[str, str]) -> Path:
    """A tree shaped like the parts of doppler this gate reads."""
    src = tmp_path / "native" / "src" / "app" / "wfmgen.c"
    src.parent.mkdir(parents=True, exist_ok=True)
    body = "static const opt_t OPTS[] = {\n"
    for f in [*ANCHORS, *flags]:
        body += _row(f, "-o" if f == "--output" else None)
    src.write_text(body + "};\n", encoding="utf-8")

    guide = tmp_path / "docs" / "guide" / "wfmgen"
    guide.mkdir(parents=True, exist_ok=True)
    for name, text in pages.items():
        (guide / name).write_text(text, encoding="utf-8")
    return tmp_path


def _run(root: Path):
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


# Every fixture page has to name the anchors, or they dominate the report.
ANCHOR_DOCS = "`--type` `--count` `--output` `-o` `--freq`\n"


def test_a_fully_documented_guide_passes(tmp_path: Path) -> None:
    """The state the gate is holding: every accepted flag is findable."""
    root = _seed(
        tmp_path,
        ["--asm"],
        {"coding.md": ANCHOR_DOCS + "`--asm` attaches the marker.\n"},
    )
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr
    # four anchors + the `-o` alias + `--asm`: an alias counts
    assert "6 flag(s) documented" in r.stdout


def test_an_undocumented_flag_fails(tmp_path: Path) -> None:
    """The defect the gate exists for: a flag that reached no page."""
    root = _seed(tmp_path, ["--asm"], {"coding.md": ANCHOR_DOCS})
    r = _run(root)
    assert r.returncode == 1
    assert "--asm" in r.stderr
    # the report has to name the flag, or the reader goes hunting for which
    assert "1 of 6" in r.stderr


def test_an_alias_is_a_flag(tmp_path: Path) -> None:
    """An alias exists to be typed; undocumented, it is a secret.

    This is the case the gate found on its first run against the real repo:
    `--randomise` was documented at length and `--randomize`, which the
    parser accepts, appeared in none of the eleven pages.
    """
    root = _seed(tmp_path, [], {"coding.md": ANCHOR_DOCS.replace("`-o` ", "")})
    r = _run(root)
    assert r.returncode == 1
    assert "-o" in r.stderr


def test_a_longer_flag_does_not_document_its_prefix(tmp_path: Path) -> None:
    """`--interleave-unit` must not count as documenting `--interleave`.

    Both are live wfmgen flags, so a substring test would report the guide
    complete while the shorter one sat unmentioned -- the gate would be
    green precisely because the two names look alike.
    """
    root = _seed(
        tmp_path,
        ["--interleave", "--interleave-unit"],
        {"coding.md": ANCHOR_DOCS + "`--interleave-unit` sets the span.\n"},
    )
    r = _run(root)
    assert r.returncode == 1
    assert "--interleave\n" in r.stderr
    assert "--interleave-unit" not in r.stderr.split("but named in")[1]


def test_no_pages_at_all_fails_closed(tmp_path: Path) -> None:
    """An empty guide makes every flag vacuously documented.

    Left to glob, the gate would print OK having read nothing -- the exact
    shape of a check that passes because it never ran.
    """
    root = _seed(tmp_path, ["--asm"], {})
    r = _run(root)
    assert r.returncode == 1
    assert "no pages" in r.stderr


def test_a_new_page_counts_without_being_registered(tmp_path: Path) -> None:
    """Pages are discovered, so the gate cannot be satisfied by a list."""
    root = _seed(tmp_path, ["--asm"], {"coding.md": ANCHOR_DOCS})
    assert _run(root).returncode == 1
    (root / "docs" / "guide" / "wfmgen" / "brand-new.md").write_text(
        "`--asm` attaches the marker.\n", encoding="utf-8"
    )
    r = _run(root)
    assert r.returncode == 0, r.stdout + r.stderr


def test_broken_flag_discovery_is_not_a_pass(tmp_path: Path) -> None:
    """If the option table stops parsing, say so -- do not report zero gaps.

    Shares `dispatcher_flags` with the flag-matrix gate precisely so this
    failure mode has one implementation and one message.
    """
    root = _seed(tmp_path, ["--asm"], {"coding.md": ANCHOR_DOCS})
    src = root / "native" / "src" / "app" / "wfmgen.c"
    src.write_text(src.read_text().replace(".name  =", ".flagname ="))
    r = _run(root)
    assert r.returncode != 0
    assert "flag discovery is broken" in r.stdout + r.stderr


def test_the_real_repo_passes_its_own_gate() -> None:
    """The gate, applied to doppler itself -- the artifact, not a fixture."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout + r.stderr
