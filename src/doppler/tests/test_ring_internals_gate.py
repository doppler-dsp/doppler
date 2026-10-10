"""The ring-internals gate, exercised over seeded trees (doppler#1426).

acq, detector and detector2d each rebuilt the ring's operations from its
struct -- free space as `capacity - (head - tail)`, a frame pointer as
`data + (tail & mask) * 2`, a reset as two stores -- until #1895 moved them
onto its API. `scripts/check_ring_internals.py` holds them there: outside
buffer.h, no code touches a ring's head, tail or mask, except what its
shrink-only baseline still lists. Each case seeds a tree and points
`--root` at it; the ratchet cases make it a git checkout with a base; the
last runs the real make target on the real tree.
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
SCRIPT = REPO / "scripts" / "check_ring_internals.py"
BUFFER_H = "native/inc/doppler/buffer/buffer.h"
BASELINE = "scripts/.ring-internals-ratchet"
#: buffer.h's own instantiations, as the gate reads them.
_OWNER = "DECLARE_DP_BUFFER (f32, float)\nDECLARE_DP_BUFFER (i16, int16_t)\n"
#: A consumer that frames by hand: a local ring, two accesses.
_BY_HAND = (
    "void f (dp_f32_t *ring)\n{\n"
    "  float *p = ring->data + (ring->tail & ring->mask) * 2;\n}\n"
)


def _write(root: Path, files: dict[str, str]) -> None:
    for rel, text in files.items():
        f = root / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")


def _run(root: Path, files: dict[str, str], *args: str):
    _write(root, {BUFFER_H: _OWNER, **files})
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *args],
        capture_output=True,
        text=True,
    )


# ── what is a site ───────────────────────────────────────────────────────────


@pytest.mark.parametrize(
    ("files", "said"),
    [
        # A local ring: a private frame pointer, two accesses.
        ({"native/src/x/x_core.c": _BY_HAND}, "native/src/x/x_core.c: 2"),
        # A struct member declared as a ring in a header, read anywhere:
        # burst_capture's history shape.
        (
            {
                "native/inc/doppler/y/y_core.h": "typedef struct\n{\n"
                "  dp_f32_t *hist;\n} y_t;\n",
                "native/src/y/y_core.c": "void g (y_t *s)\n{\n"
                "  DP_STORE_REL (&s->hist->head, 0u);\n}\n",
            },
            "native/src/y/y_core.c: 1",
        ),
        # A consumer's own instantiation of the macro: dp_tlm's shape.
        (
            {
                "native/inc/doppler/t/t_core.h": "DECLARE_DP_BUFFER "
                "(tlmr, uint64_t)\n",
                "native/src/t/t_core.c": "size_t h (dp_tlmr_t *r)\n{\n"
                "  return r->head - r->tail;\n}\n",
            },
            "native/src/t/t_core.c: 2",
        ),
        # An example is a consumer too.
        ({"native/examples/demo.c": _BY_HAND}, "native/examples/demo.c: 2"),
        # So are a validation harness and a benchmark.
        (
            {"native/validation/v.c": _BY_HAND},
            "native/validation/v.c: 2",
        ),
        (
            {"native/benchmarks/bench_b.c": _BY_HAND},
            "native/benchmarks/bench_b.c: 2",
        ),
        # clang-format breaks a long chain before its `->`: still one access
        # (burst_capture_core.c's shape).
        (
            {
                "native/inc/doppler/w/w_core.h": "typedef struct\n{\n"
                "  dp_f32_t *hist;\n} w_t;\n",
                "native/src/w/w_core.c": "void g (w_t *s)\n{\n"
                "  DP_STORE_REL (&s->hist\n                    ->head, 0u);\n"
                "}\n",
            },
            "native/src/w/w_core.c: 1",
        ),
    ],
)
def test_a_ring_internal_access_is_named(
    tmp_path: Path, files: dict[str, str], said: str
) -> None:
    r = _run(tmp_path, files)
    assert r.returncode == 1, r.stdout
    assert said in r.stdout, r.stdout


@pytest.mark.parametrize(
    "files",
    [
        # Not rings: a delay line's own head, an LFSR's mask -- the same
        # field names on structs buffer.h does not own.
        {
            "native/src/d/d_core.c": "void d (delay_t *state)\n{\n"
            "  state->head = (state->head - 1) & state->mask;\n}\n"
        },
        # A ring PARAMETER named in a header prototype does not make every
        # file's `state` a ring: only members do.
        {
            "native/inc/doppler/q/q_core.h": "void q (dp_f32_t *state);\n",
            "native/src/p/p_core.c": "void p (pn_t *state)\n{\n"
            "  state->reg &= state->mask;\n}\n",
        },
        # The buffer's own tests are oracles of its internals.
        {"native/tests/test_x.c": _BY_HAND},
        # Documentation that QUOTES an access is not one: comments are
        # blanked before matching, block and line alike.
        {
            "native/src/c/c_core.c": "void c (dp_f32_t *ring)\n{\n"
            "  /* never ring->tail & ring->mask by hand */\n"
            "  // nor ring->head\n"
            "  dp_f32_consume (ring, 8);\n}\n"
        },
        # And buffer.h is the owner.
        {BUFFER_H: _OWNER + "#define X(r) ((r)->tail & (r)->mask)\n"},
        # The ring's API is the point.
        {
            "native/src/z/z_core.c": "void z (dp_f32_t *ring)\n{\n"
            "  float *p = dp_f32_peek (ring, 8);\n"
            "  dp_f32_consume (ring, 8);\n}\n"
        },
    ],
)
def test_what_is_not_a_ring_internal_access_passes(
    tmp_path: Path, files: dict[str, str]
) -> None:
    r = _run(tmp_path, files)
    assert r.returncode == 0, r.stdout


# ── the baseline ─────────────────────────────────────────────────────────────


def test_a_baselined_file_passes_at_its_count(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        {
            "native/src/x/x_core.c": _BY_HAND,
            BASELINE: "native/src/x/x_core.c 2  # why\n",
        },
    )
    assert r.returncode == 0, r.stdout


@pytest.mark.parametrize(
    ("line", "body", "said"),
    [
        # One more site than the line allows.
        (
            "native/src/x/x_core.c 1  # why\n",
            _BY_HAND,
            "the baseline allows 1",
        ),
        # A count left above what the file does: it only shrinks.
        (
            "native/src/x/x_core.c 3  # why\n",
            _BY_HAND,
            "allows 3 but the file has 2",
        ),
        # Every line says why.
        ("native/src/x/x_core.c 2\n", _BY_HAND, "has no reason"),
    ],
)
def test_the_baseline_is_held_to_the_file(
    tmp_path: Path, line: str, body: str, said: str
) -> None:
    r = _run(tmp_path, {"native/src/x/x_core.c": body, BASELINE: line})
    assert r.returncode == 1, r.stdout
    assert said in r.stdout, r.stdout


# ── the ratchet, against a real merge base ───────────────────────────────────


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )


#: Two accesses on one line (tail, mask); a file of k of them has 2k.
_LINE = "  p = ring->data + (ring->tail & ring->mask) * 2;\n"


def _tree(k: int) -> str:
    return "void f (dp_f32_t *ring)\n{\n" + _LINE * k + "}\n"


def _based(root: Path) -> None:
    """A checkout whose main baselines one file at its 2 sites."""
    _write(
        root,
        {
            BUFFER_H: _OWNER,
            "scripts/check_ring_internals.py": "# the gate\n",
            "native/src/x/x_core.c": _tree(1),
            BASELINE: "native/src/x/x_core.c 2  # why\n",
        },
    )
    _git(root, "init", "-q", "-b", "main")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "base")
    _git(root, "checkout", "-q", "-b", "feature")


@pytest.mark.parametrize(
    ("files", "refused", "said"),
    [
        # Raised, with the file to match: refused against the base.
        (
            {
                "native/src/x/x_core.c": _tree(2),
                BASELINE: "native/src/x/x_core.c 4  # why\n",
            },
            True,
            "'native/src/x/x_core.c' rose to 4",
        ),
        # A new file, listed with its own line: refused too.
        (
            {
                "native/src/w/w_core.c": _tree(1),
                BASELINE: "native/src/x/x_core.c 2  # why\n"
                "native/src/w/w_core.c 2  # why\n",
            },
            True,
            "'native/src/w/w_core.c' rose to 2",
        ),
        # Shrunk, with the file to match: the point.
        (
            {
                "native/src/x/x_core.c": "void f (dp_f32_t *ring)\n{\n"
                "  p = ring->data + (ring->tail & 7) * 2;\n}\n",
                BASELINE: "native/src/x/x_core.c 1  # why\n",
            },
            False,
            "ring-internals: OK",
        ),
    ],
)
def test_the_baseline_only_shrinks_against_the_base(
    tmp_path: Path, files: dict[str, str], refused: bool, said: str
) -> None:
    _based(tmp_path)
    assert _run(tmp_path, {}, "--base", "main").returncode == 0
    _write(tmp_path, files)
    _git(tmp_path, "add", "-A")
    r = _run(tmp_path, {}, "--base", "main")
    assert (r.returncode == 1) == refused, r.stdout
    assert said in r.stdout, r.stdout
    # The file matches its line in every case: only the RISE is refused.
    assert "the baseline allows" not in r.stdout, r.stdout
    assert "but the file has" not in r.stdout, r.stdout


def test_a_base_that_will_not_resolve_has_not_passed(tmp_path: Path) -> None:
    """A ratchet that cannot read its baseline fails closed."""
    _based(tmp_path)
    r = _run(tmp_path, {}, "--base", "no-such-ref")
    assert r.returncode == 1, r.stdout
    assert "cannot resolve no-such-ref" in r.stdout


def test_the_live_tree_passes_through_make() -> None:
    """The real target on the real tree.

    ``RING_INTERNALS_BASE=HEAD`` neuters only the merge-base comparison,
    whose execution home is ``make lint-ring-internals`` at its default
    origin/main in the pre-commit job (fetch-depth 0). This runs in the
    Python job, whose shallow checkout has no origin/main.
    """
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "lint-ring-internals",
            "RING_INTERNALS_BASE=HEAD",
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "ring-internals: OK" in r.stdout
