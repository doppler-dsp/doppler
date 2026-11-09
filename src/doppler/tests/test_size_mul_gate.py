"""The size-mul gate, exercised over seeded trees.

A size product that wraps allocates small and is then written past, and
the hand-written guard against it (`n > SIZE_MAX / k`) was spelled a
different way at every site. `scripts/check_size_mul_sites.py` holds the
guard to its one home, dp_size_mul() in clib_common.h: no `SIZE_MAX /`
in library C except what its shrink-only ratchet still lists. Each case
seeds a tree and points `--root` at it; the ratchet cases make it a git
checkout with a base; the last runs the real make target on the real tree.
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
SCRIPT = REPO / "scripts" / "check_size_mul_sites.py"
OWNER = "native/inc/doppler/clib_common.h"
BASELINE = "scripts/.size-mul-ratchet"
#: A hand-written multiply guard: one site.
_GUARD = (
    "void *f (size_t x, size_t y)\n{\n"
    "  if (x > SIZE_MAX / y)\n    return NULL;\n"
    "  return malloc (x * y);\n}\n"
)


def _write(root: Path, files: dict[str, str]) -> None:
    for rel, text in files.items():
        f = root / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")


def _run(root: Path, files: dict[str, str], *args: str):
    _write(root, files)
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *args],
        capture_output=True,
        text=True,
    )


# ── what is a site ───────────────────────────────────────────────────────────


@pytest.mark.parametrize(
    ("files", "said"),
    [
        # A stray guard in library source.
        (
            {"native/src/x/x_core.c": _GUARD},
            "native/src/x/x_core.c:3: if (x > SIZE_MAX / y)  -- use "
            "dp_size_mul (dp_size_add for a sum) (clib_common.h)",
        ),
        # A public header other than the home.
        (
            {
                "native/inc/doppler/y/y_core.h": "#define Y(n) "
                "((n) > SIZE_MAX / 8)\n"
            },
            "native/inc/doppler/y/y_core.h: 1 hand-written",
        ),
        # A validation harness is library C too.
        ({"native/validation/v.c": _GUARD}, "native/validation/v.c: 1"),
        # So is C under ffi/.
        ({"ffi/c/shim.c": _GUARD}, "ffi/c/shim.c: 1"),
        # clang-format may break before the `/`: still one site.
        (
            {
                "native/src/w/w_core.c": "int w (size_t n)\n{\n"
                "  return n > SIZE_MAX\n"
                "                / 4;\n}\n"
            },
            "native/src/w/w_core.c: 1",
        ),
        # A double checked against SIZE_MAX: on LP64 the cast rounds up to
        # 2^64, so 2^64 passes. That is dp_size_from_double's job.
        (
            {
                "native/src/d/d_core.c": "int d (double x)\n{\n"
                "  return x >= 0.0 && x <= (double)SIZE_MAX;\n}\n"
            },
            "native/src/d/d_core.c:3: return x >= 0.0 && x <= "
            "(double)SIZE_MAX;  -- use dp_size_from_double (clib_common.h)",
        ),
        # Spelled with a parenthesised quotient, it is one site, not two.
        (
            {
                "native/src/e/e_core.c": "int e (double r)\n{\n"
                "  return r > (double) (SIZE_MAX / 2);\n}\n"
            },
            "native/src/e/e_core.c: 1 hand-written",
        ),
        # SIZE_MAX in any numerator, spelled any way C spells it: the
        # multiply-add guard buffer.h's framer used, a parenthesised
        # macro, and the two all-ones casts.
        (
            {
                "native/src/m/m_core.c": "int m (size_t k, size_t f, "
                "size_t h)\n{\n  return k <= (SIZE_MAX - f) / h;\n}\n"
            },
            "native/src/m/m_core.c:3: return k <= (SIZE_MAX - f) / h;  -- "
            "use dp_size_mul (dp_size_add for a sum)",
        ),
        (
            {
                "native/src/m/m_core.c": "int m (size_t k)\n{\n"
                "  return k > (SIZE_MAX) / 4;\n}\n"
            },
            "native/src/m/m_core.c: 1 hand-written",
        ),
        (
            {
                "native/src/m/m_core.c": "int m (size_t k)\n{\n"
                "  return k > ~(size_t)0 / 4;\n}\n"
            },
            "native/src/m/m_core.c: 1 hand-written",
        ),
        (
            {
                "native/src/m/m_core.c": "int m (size_t k)\n{\n"
                "  return k > (size_t)-1 / 4;\n}\n"
            },
            "native/src/m/m_core.c: 1 hand-written",
        ),
        # A cast of SIZE_MAX to any floating type.
        (
            {
                "native/src/g/g_core.c": "int g (float x)\n{\n"
                "  return x <= (float)SIZE_MAX;\n}\n"
            },
            "use dp_size_from_double",
        ),
        (
            {
                "native/src/g/g_core.c": "int g (long double x)\n{\n"
                "  return x <= (long double) SIZE_MAX;\n}\n"
            },
            "use dp_size_from_double",
        ),
        # The implicit conversion, in a clause that says it is floating.
        (
            {
                "native/src/g/g_core.c": "int g (double x)\n{\n"
                "  return x * 1.0 <= SIZE_MAX;\n}\n"
            },
            "native/src/g/g_core.c:3: return x * 1.0 <= SIZE_MAX;  -- use "
            "dp_size_from_double",
        ),
        (
            {
                "native/src/g/g_core.c": "int g (size_t n, double r)\n{\n"
                "  return SIZE_MAX > (double)n * r;\n}\n"
            },
            "use dp_size_from_double",
        ),
        # The rest of C's spellings of all-ones.
        *[
            (
                {
                    "native/src/m/m_core.c": "int m (size_t k)\n{\n"
                    f"  return k > {spelling} / 4;\n}}\n"
                },
                "native/src/m/m_core.c: 1 hand-written",
            )
            for spelling in (
                "(size_t)~0",
                "(size_t)(-1)",
                "~((size_t)0)",
                "(size_t)-1u",
                "(size_t)-1L",
                "(size_t)-1UL",
                "~(size_t)0u",
            )
        ],
    ],
)
def test_a_hand_written_guard_is_named(
    tmp_path: Path, files: dict[str, str], said: str
) -> None:
    r = _run(tmp_path, files)
    assert r.returncode == 1, r.stdout
    assert said in r.stdout, r.stdout


@pytest.mark.parametrize(
    "files",
    [
        # The home itself.
        {
            OWNER: "static inline int\ndp_size_mul (size_t a, size_t b, "
            "size_t *out)\n{\n  if (b != 0 && a > SIZE_MAX / b)\n"
            "    return 0;\n  *out = a * b;\n  return 1;\n}\n"
        },
        # Tests build boundary values on purpose.
        {"native/tests/test_x.c": "size_t forged = SIZE_MAX / 16 + 3;\n"},
        {"native/tests/x/test_x_core.c": _GUARD},
        # A jm:generated file is the generator's to fix.
        {"native/src/r/r_ext_q.c": "/* jm:generated r_ext_q.c */\n" + _GUARD},
        # Vendored code is not ours.
        {"native/vendor/lib/z.c": _GUARD},
        # A comment that quotes the idiom is not one.
        {
            "native/src/c/c_core.c": "/* never x > SIZE_MAX / y by hand */\n"
            "// nor SIZE_MAX / k\n"
            "int c (size_t a, size_t b, size_t *o)\n{\n"
            "  return dp_size_mul (a, b, o);\n}\n"
        },
        # A SIZE_MAX that is not divided is not a product: a range check,
        # a saturating sum.
        {
            "native/src/s/s_core.c": "int s (uint64_t len, size_t a)\n{\n"
            "  if (len > SIZE_MAX) return 0;\n"
            "  return a > SIZE_MAX - 1;\n}\n"
        },
        # A double compared with a power of two, not with SIZE_MAX.
        {
            "native/src/f/f_core.c": "int f (double x)\n{\n"
            "  return x < 0x1p64;\n}\n"
        },
        # A width, not an all-ones cast: sizeof (size_t) - 1.
        {
            "native/src/f/f_core.c": "size_t f (void)\n{\n"
            "  return ((size_t)1 << (CHAR_BIT * sizeof (size_t) - 1)) / 8;"
            "\n}\n"
        },
        # A brace-less `if`: the `)` closing its controlling expression
        # ends the comparison's context, so the 1.0 it returns is not in it.
        {
            "native/src/f/f_core.c": "double f (size_t n)\n{\n"
            "  if (n == SIZE_MAX) return 1.0;\n  return 0.0;\n}\n"
        },
        # A float literal in ANOTHER clause does not make SIZE_MAX's
        # comparison floating.
        {
            "native/src/f/f_core.c": "int f (float t, size_t hi)\n{\n"
            "  return t != 0.0f || hi != SIZE_MAX;\n}\n"
        },
    ],
)
def test_what_is_not_a_hand_written_guard_passes(
    tmp_path: Path, files: dict[str, str]
) -> None:
    r = _run(tmp_path, files)
    assert r.returncode == 0, r.stdout


# ── the ratchet ──────────────────────────────────────────────────────────────


def test_a_ratcheted_file_passes_at_its_count(tmp_path: Path) -> None:
    r = _run(
        tmp_path,
        {
            "native/src/x/x_core.c": _GUARD,
            BASELINE: "native/src/x/x_core.c 1  # why\n",
        },
    )
    assert r.returncode == 0, r.stdout
    assert "size-mul: OK -- 1 ratcheted site(s)" in r.stdout


@pytest.mark.parametrize(
    ("line", "said"),
    [
        # One more site than the line allows.
        ("native/src/x/x_core.c 0  # why\n", "the ratchet allows 0"),
        # A count left above what the file has: it only shrinks.
        ("native/src/x/x_core.c 3  # why\n", "allows 3 but the file has 1"),
        # Every line says why.
        ("native/src/x/x_core.c 1\n", "has no reason"),
    ],
)
def test_the_ratchet_is_held_to_the_file(
    tmp_path: Path, line: str, said: str
) -> None:
    r = _run(tmp_path, {"native/src/x/x_core.c": _GUARD, BASELINE: line})
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


def _tree(k: int) -> str:
    return "int f (size_t x)\n{\n" + "  g (x > SIZE_MAX / 3);\n" * k + "}\n"


def _based(root: Path) -> None:
    """A checkout whose main ratchets one file at its 2 sites."""
    _write(
        root,
        {
            "scripts/check_size_mul_sites.py": "# the gate\n",
            "native/src/x/x_core.c": _tree(2),
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
                "native/src/x/x_core.c": _tree(3),
                BASELINE: "native/src/x/x_core.c 3  # why\n",
            },
            True,
            "'native/src/x/x_core.c' rose to 3",
        ),
        # A new file, listed with its own line: refused too.
        (
            {
                "native/src/w/w_core.c": _tree(1),
                BASELINE: "native/src/x/x_core.c 2  # why\n"
                "native/src/w/w_core.c 1  # why\n",
            },
            True,
            "'native/src/w/w_core.c' rose to 1",
        ),
        # Shrunk, with the file to match: the point.
        (
            {
                "native/src/x/x_core.c": _tree(1),
                BASELINE: "native/src/x/x_core.c 1  # why\n",
            },
            False,
            "size-mul: OK",
        ),
    ],
)
def test_the_ratchet_only_shrinks_against_the_base(
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
    assert "the ratchet allows" not in r.stdout, r.stdout
    assert "but the file has" not in r.stdout, r.stdout


def test_a_base_that_will_not_resolve_has_not_passed(tmp_path: Path) -> None:
    """A ratchet that cannot read its baseline fails closed."""
    _based(tmp_path)
    r = _run(tmp_path, {}, "--base", "no-such-ref")
    assert r.returncode == 1, r.stdout
    assert "cannot resolve no-such-ref" in r.stdout


def test_the_live_tree_passes_through_make() -> None:
    """The real target on the real tree.

    ``SIZE_MUL_BASE=HEAD`` neuters only the merge-base comparison, whose
    execution home is ``make lint-size-mul`` at its default origin/main in
    the pre-commit job (fetch-depth 0). This runs in the Python job, whose
    shallow checkout has no origin/main.
    """
    r = subprocess.run(
        [
            "make",
            "-s",
            "-C",
            str(REPO),
            "lint-size-mul",
            "SIZE_MUL_BASE=HEAD",
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "size-mul: OK" in r.stdout


def test_a_second_guard_on_a_ratcheted_line_raises_the_count(
    tmp_path: Path,
) -> None:
    """Sites are occurrences, not lines: appending a guard to a line the
    ratchet already counts is a new site, and refused."""
    one = "int f (size_t a, size_t b)\n{\n  return a > SIZE_MAX / b;\n}\n"
    two = (
        "int f (size_t a, size_t b)\n{\n"
        "  return a > SIZE_MAX / b || b > SIZE_MAX / 4;\n}\n"
    )
    line = "native/src/x/x_core.c 1  # why\n"
    assert (
        _run(
            tmp_path, {"native/src/x/x_core.c": one, BASELINE: line}
        ).returncode
        == 0
    )
    r = _run(tmp_path, {"native/src/x/x_core.c": two, BASELINE: line})
    assert r.returncode == 1, r.stdout
    assert "2 hand-written SIZE_MAX guard(s), the ratchet allows 1" in r.stdout
