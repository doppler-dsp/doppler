"""The allocation-helper gate, exercised over a seeded tree.

`scripts/check_alloc_helpers.py` exists because the rule it enforces spent a
month enforced by nothing. `native/inc/doppler/clib_common.h` gained
`dp_xmalloc`,
`dp_xcalloc` and `dp_xnn` on 2026-07-21, three cores adopted them, and the
instruction to extend that to other cores as they were touched lived in a
memory file and in no gate. The check that found this asked a simpler
question than the rule does -- *does anything at all fail on a bare
`malloc`?* -- and the answer was no.

So the cases below are about the gate's own failure modes rather than about
allocation. Each one seeds a fake repo, because a gate that can only be
tested against the real tree cannot be sabotaged: you would have to break
doppler to check it, and nobody does that twice.

The ratchet has two directions and both are load-bearing. Growth is the
obvious one. Slack -- a count left high after its file improved -- is the one
that shipped broken elsewhere in this repo: `PY_HOLLOW_ALLOW` had no
staleness check, so an entry kept its waiver after the file it covered
started recording, and the next regression in that file would have passed.
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
SCRIPT = REPO / "scripts" / "check_alloc_helpers.py"


def _seed(tmp_path: Path, files: dict[str, str], allow: str | None) -> Path:
    """A minimal tree shaped like the parts of doppler the gate scans."""
    for rel, body in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    a = tmp_path / "scripts" / ".alloc-helper-allow"
    a.parent.mkdir(parents=True, exist_ok=True)
    a.write_text(allow if allow is not None else "", encoding="utf-8")
    return a


def _run(tmp_path: Path, allow_path: Path, *extra: str):
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(tmp_path),
            "--allow",
            str(allow_path),
            *extra,
        ],
        capture_output=True,
        text=True,
    )


CLEAN = "void f (void) { s->b = dp_xmalloc (n); }\n"
BARE = "void f (void) { s->b = malloc (n); }\n"


def test_a_helper_only_tree_passes(tmp_path: Path) -> None:
    """The state the gate is steering towards: no bare allocation at all."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="")
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout
    assert "0 bare allocation" in r.stdout


def test_a_new_bare_allocation_fails(tmp_path: Path) -> None:
    """The defect the gate exists for, in a file nothing has allowed."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": BARE}, allow="")
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "not in the baseline" in r.stdout
    # the message has to name the line, or the gate reports a problem the
    # reader then has to go and find
    assert "native/src/x/x_core.c:1" in r.stdout
    assert "dp_xmalloc" in r.stdout


def test_one_more_than_the_baseline_fails(tmp_path: Path) -> None:
    """A ratcheted file may not GROW -- the common case, and the subtle one.

    The file is already allowed to allocate, so a reader adding one more
    would see a green gate under any check that asked only "is this file
    listed?".
    """
    a = _seed(
        tmp_path,
        {"native/src/x/x_core.c": BARE * 3},
        allow="native/src/x/x_core.c 2\n",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "baseline allows 2" in r.stdout


def test_the_baseline_may_not_go_slack(tmp_path: Path) -> None:
    """A count left high after its file improved is a waiver with no reason."""
    a = _seed(
        tmp_path,
        {"native/src/x/x_core.c": BARE},
        allow="native/src/x/x_core.c 5\n",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "gone slack" in r.stdout
    assert "has to come down" in r.stdout


def test_a_listed_file_that_vanished_fails(tmp_path: Path) -> None:
    """A deleted file's allowance must not sit there covering nothing."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="gone/y.c 1\n")
    r = _run(tmp_path, a)
    assert r.returncode == 1
    assert "the file is gone" in r.stdout


def test_the_helpers_own_home_is_exempt(tmp_path: Path) -> None:
    """`dp_xmalloc` IS `dp_xnn (malloc (n))`; one file has to contain it."""
    a = _seed(
        tmp_path,
        {
            "native/inc/doppler/clib_common.h": "void *dp_xmalloc (size_t n)"
            " { return dp_xnn (malloc (n)); }\n"
        },
        allow="",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_generated_glue_is_out_of_scope(tmp_path: Path) -> None:
    """jm owns `_ext` files, and their NULL checks are reachable.

    A numpy output buffer is sized by the CALLER, so its allocation can fail
    on a length a caller chose and the generated code raises MemoryError.
    Gating that would be a gate against the generator.
    """
    a = _seed(
        tmp_path,
        {
            "native/src/coding/coding_ext.c": BARE,
            "native/src/coding/coding_ext_interleaver.c": BARE,
        },
        allow="",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_the_helper_call_itself_is_not_a_hit(tmp_path: Path) -> None:
    """`dp_xmalloc (` contains `malloc (` and must not match it.

    Without the look-behind, adopting the helper would RAISE a file's count
    and the gate would fight the fix it exists to encourage.
    """
    body = "a = dp_xmalloc (n);\nb = dp_xcalloc (n, s);\n"
    a = _seed(tmp_path, {"native/src/x/x_core.c": body}, allow="")
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_tests_and_benchmarks_are_out_of_scope(tmp_path: Path) -> None:
    """An oracle allocates freely; a crash there is a test failure."""
    a = _seed(
        tmp_path,
        {"native/tests/test_x_core.c": BARE * 4},
        allow="",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout


def test_update_baseline_records_what_is_there(tmp_path: Path) -> None:
    """The escape hatch, and that it writes a ratchet a reader can follow."""
    a = _seed(tmp_path, {"native/src/x/x_core.c": BARE * 2}, allow="")
    r = _run(tmp_path, a, "--update-baseline")
    assert r.returncode == 0, r.stdout
    assert "native/src/x/x_core.c 2" in a.read_text(encoding="utf-8")
    assert "MAY ONLY SHRINK" in a.read_text(encoding="utf-8")
    # and the tree it just recorded now passes
    assert _run(tmp_path, a).returncode == 0


def test_the_real_repo_passes_its_own_ratchet() -> None:
    """The gate, applied to doppler itself -- the artifact, not a fixture.

    `--base HEAD` deliberately neuters the RAISE check here. Whether a count
    went up without a reason is a question about a BRANCH against main, and
    it already has an execution home: `make lint-alloc-helpers`, which the
    `alloc-helpers` pre-commit hook dispatches to at the default
    `--base origin/main`, under the `pre-commit (lint + format)` job -- the
    one that checks out at `fetch-depth: 0`. This test asks the other -- do
    the counts in the tree match the baseline -- and it runs in the Python
    job, whose shallow checkout has no `origin/main` to read. Left at the
    default it would not test the ratchet; it would test the fetch depth.
    """
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "HEAD"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr


# --------------------------------------------------------------------------- #
# The RAISE check, and the two ways "no baseline" differ                      #
# --------------------------------------------------------------------------- #
def _git(tmp_path: Path, *args: str) -> None:
    subprocess.run(
        ["git", *args], cwd=tmp_path, capture_output=True, check=True
    )


def test_a_raise_without_a_reason_fails(tmp_path: Path) -> None:
    """The whole point: a count may go up, but not silently.

    The gate cannot see a raise from the working tree alone -- the allow file
    IS the baseline, so `2` and a raised `1` are the same bytes. It reads the
    file at the base ref to tell them apart.
    """
    a = _seed(
        tmp_path,
        {"native/src/x/x_core.c": BARE},
        allow="native/src/x/x_core.c 1\n",
    )
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(
        tmp_path,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    _git(tmp_path, "branch", "-f", "origin/main", "main")

    # Two bare allocations now, and the baseline raised to match -- silently.
    (tmp_path / "native/src/x/x_core.c").write_text(
        BARE + BARE, encoding="utf-8"
    )
    a.write_text("native/src/x/x_core.c 2\n", encoding="utf-8")
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 1, r.stdout
    assert "went UP with no reason" in r.stdout

    # The same raise, explained on the line, is the sanctioned path.
    a.write_text(
        "native/src/x/x_core.c 2  # caller-sized, failure handled\n",
        encoding="utf-8",
    )
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 0, r.stdout


def test_a_repo_whose_base_ref_is_missing_fails(tmp_path: Path) -> None:
    """A shallow clone must not read as "nothing was raised".

    This is the case the not-a-repo escape below must NOT swallow: the
    baseline exists, it simply could not be read, and a ratchet that cannot
    read its baseline has not passed.
    """
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="")
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(
        tmp_path,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    r = _run(tmp_path, a, "--base", "origin/does-not-exist")
    assert r.returncode == 1, r.stdout
    assert "cannot read" in r.stdout


def test_a_tree_that_is_not_a_repo_skips_the_raise_check(
    tmp_path: Path,
) -> None:
    """...while a non-repo tree has no history to have raised anything in.

    Every other test here seeds exactly that, so getting this wrong took the
    whole suite red rather than one case -- which is how it was caught.
    """
    a = _seed(tmp_path, {"native/src/x/x_core.c": CLEAN}, allow="")
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 0, r.stdout


# --------------------------------------------------------------------------- #
# The second rule: a size written as an inline product                        #
# --------------------------------------------------------------------------- #
#: An allocation sized by a product that can wrap.
PRODUCT = "void f (void) { s->b = malloc (n * sizeof *s->b); }\n"
PRODUCTS = "scripts/.alloc-product-ratchet"


def _seed_products(
    tmp_path: Path, files: dict[str, str], bare: str, products: str = ""
) -> Path:
    """A tree for the product rule: the bare-allocation allowance is given
    explicitly, so the case isolates the rule it is about."""
    a = _seed(tmp_path, files, allow=bare)
    (tmp_path / PRODUCTS).write_text(products, encoding="utf-8")
    return a


@pytest.mark.parametrize(
    ("body", "said"),
    [
        # The defect: a product that wraps, in a file nothing lists.
        (PRODUCT, "native/src/x/x_core.c:1"),
        # A sizeof first, then the count: still a product.
        (
            "void f (void) { p = malloc (sizeof (float) * n); }\n",
            "the ratchet allows 0",
        ),
        # realloc's size is its SECOND argument.
        ("void f (void) { p = realloc (p, n * 8); }\n", "allows 0"),
        # The aborting helpers are no exception: the product still wraps.
        (
            "void f (void) { p = dp_xmalloc (n * sizeof *p); }\n",
            "allows 0",
        ),
        (
            "void f (void) { p = dp_xrealloc (p, (n) * 4); }\n",
            "allows 0",
        ),
        # calloc checks nmemb * size, but not a product INSIDE either
        # argument: `a * b` has wrapped before calloc sees it.
        ("void f (void) { p = calloc (a * b, sizeof *p); }\n", "allows 0"),
        ("void f (void) { p = calloc (n, a * b); }\n", "allows 0"),
        (
            "void f (void) { p = dp_xcalloc (2 * cap, sizeof *p); }\n",
            "allows 0",
        ),
        # The checked helpers take the factors; a product passed as one of
        # them wraps just the same.
        (
            "void f (void) { p = dp_xmallocn (a * b, sizeof *p); }\n",
            "allows 0",
        ),
        (
            "void f (void) { p = dp_xreallocn (p, n, a * b); }\n",
            "allows 0",
        ),
        # clang-format breaks a long call across lines: the call is read
        # whole, and reported where it starts.
        (
            "void f (void)\n{\n  p = malloc (\n      n\n      * sizeof "
            "*p);\n}\n",
            "native/src/x/x_core.c:3",
        ),
    ],
)
def test_an_inline_product_size_is_named(
    tmp_path: Path, body: str, said: str
) -> None:
    a = _seed_products(
        tmp_path, {"native/src/x/x_core.c": body}, "native/src/x/x_core.c 1\n"
    )
    r = _run(tmp_path, a)
    assert r.returncode == 1, r.stdout
    assert "sized by an inline product" in r.stdout, r.stdout
    assert said in r.stdout, r.stdout
    assert "dp_mallocn / dp_xmallocn / dp_xreallocn" in r.stdout


@pytest.mark.parametrize(
    ("files", "bare"),
    [
        # The checked helpers are the point.
        (
            {
                "native/src/x/x_core.c": "void f (void)\n{\n"
                "  a = dp_xmallocn (n, sizeof *a);\n"
                "  b = dp_mallocn (n, sizeof *b);\n"
                "  c = dp_xreallocn (c, n, sizeof *c);\n}\n"
            },
            "",
        ),
        # calloc checks nmemb * size itself when each is a plain value.
        (
            {
                "native/src/x/x_core.c": "void f (void) "
                "{ p = calloc (n, sizeof *p); }\n"
            },
            "native/src/x/x_core.c 1\n",
        ),
        # A dereference after sizeof, a cast and a product inside
        # parentheses are not the size's own product.
        (
            {
                "native/src/x/x_core.c": "void f (void)\n{\n"
                "  a = malloc (sizeof *a);\n"
                "  b = (int *)malloc (bytes);\n"
                "  c = malloc (sizeof (struct { int *q; }));\n"
                "  d = malloc (round_up (n * 4));\n}\n"
            },
            "native/src/x/x_core.c 4\n",
        ),
        # A jm:generated file is the generator's to fix.
        (
            {
                "native/src/x/x_core.c": "/* jm:generated x_core.c */\n"
                + PRODUCT
            },
            "native/src/x/x_core.c 1\n",
        ),
        # Comments that quote the idiom are not one.
        (
            {
                "native/src/x/x_core.c": "/* never malloc (n * size) */\n"
                "// nor realloc (p, n * size)\n"
            },
            # the bare rule reads raw text, so it counts both quotes
            "native/src/x/x_core.c 2\n",
        ),
        # Tests are oracles.
        ({"native/tests/test_x.c": PRODUCT}, ""),
    ],
)
def test_what_is_not_an_inline_product_passes(
    tmp_path: Path, files: dict[str, str], bare: str
) -> None:
    a = _seed_products(tmp_path, files, bare)
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout
    assert "alloc-product gate: OK" in r.stdout


def test_a_ratcheted_product_passes_at_its_count(tmp_path: Path) -> None:
    a = _seed_products(
        tmp_path,
        {"native/src/x/x_core.c": PRODUCT * 2},
        "native/src/x/x_core.c 2\n",
        "native/src/x/x_core.c 2\n",
    )
    r = _run(tmp_path, a)
    assert r.returncode == 0, r.stdout
    assert "2 inline-product size(s) across 1 file(s)" in r.stdout


@pytest.mark.parametrize(
    ("products", "said"),
    [
        # One more site than the line allows.
        ("native/src/x/x_core.c 1\n", "the ratchet allows 1"),
        # A count left above what the file has: it only shrinks.
        ("native/src/x/x_core.c 3\n", "allows 3 but the file has 2"),
    ],
)
def test_the_product_ratchet_is_held_to_the_file(
    tmp_path: Path, products: str, said: str
) -> None:
    a = _seed_products(
        tmp_path,
        {"native/src/x/x_core.c": PRODUCT * 2},
        "native/src/x/x_core.c 2\n",
        products,
    )
    r = _run(tmp_path, a)
    assert r.returncode == 1, r.stdout
    assert said in r.stdout, r.stdout


def test_a_raised_product_count_fails_even_with_a_reason(
    tmp_path: Path,
) -> None:
    """Unlike the bare rule, a raise has no escape: every site has a
    checked spelling."""
    a = _seed_products(
        tmp_path,
        {"native/src/x/x_core.c": PRODUCT},
        "native/src/x/x_core.c 1\n",
        "native/src/x/x_core.c 1\n",
    )
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(
        tmp_path,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    _git(tmp_path, "branch", "-f", "origin/main", "main")
    assert _run(tmp_path, a, "--base", "origin/main").returncode == 0

    (tmp_path / "native/src/x/x_core.c").write_text(
        PRODUCT * 2, encoding="utf-8"
    )
    # the bare rule's own raise, explained, so only the product rule speaks
    a.write_text("native/src/x/x_core.c 2  # caller-sized\n", encoding="utf-8")
    (tmp_path / PRODUCTS).write_text(
        "native/src/x/x_core.c 2  # a reason does not help\n",
        encoding="utf-8",
    )
    r = _run(tmp_path, a, "--base", "origin/main")
    assert r.returncode == 1, r.stdout
    assert "'native/src/x/x_core.c' rose to 2" in r.stdout
    # the file matches its line: only the RISE is refused
    assert "the ratchet allows" not in r.stdout


def test_update_baseline_records_the_products(tmp_path: Path) -> None:
    a = _seed_products(
        tmp_path, {"native/src/x/x_core.c": PRODUCT * 3}, "", ""
    )
    r = _run(tmp_path, a, "--update-baseline")
    assert r.returncode == 0, r.stdout
    text = (tmp_path / PRODUCTS).read_text(encoding="utf-8")
    assert "native/src/x/x_core.c 3" in text
    assert "MAY ONLY SHRINK" in text
    assert _run(tmp_path, a).returncode == 0


def test_a_renamed_product_list_cannot_hide_a_raise(tmp_path: Path) -> None:
    """The list's `since` is the rule's own module, not the list: a raise
    made in the same change as moving the list to a new path is read
    against an EMPTY list at the base, so every entry is added, and the
    rise is refused. With the list as its own `since`, the moved list was
    "new", nothing counted as added, and the raise passed."""
    a = _seed_products(
        tmp_path,
        {
            "native/src/x/x_core.c": PRODUCT,
            "scripts/_alloc_products.py": "# the rule\n",
        },
        "native/src/x/x_core.c 1\n",
        "native/src/x/x_core.c 1\n",
    )
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(
        tmp_path,
        "-c",
        "user.email=t@t",
        "-c",
        "user.name=t",
        "commit",
        "-qm",
        "base",
    )
    _git(tmp_path, "branch", "-f", "origin/main", "main")

    (tmp_path / "native/src/x/x_core.c").write_text(
        PRODUCT * 2, encoding="utf-8"
    )
    a.write_text("native/src/x/x_core.c 2  # caller-sized\n", encoding="utf-8")
    moved = tmp_path / "scripts" / ".alloc-product-ratchet-moved"
    moved.write_text("native/src/x/x_core.c 2\n", encoding="utf-8")
    (tmp_path / PRODUCTS).unlink()
    r = _run(tmp_path, a, "--base", "origin/main", "--products", str(moved))
    assert r.returncode == 1, r.stdout
    assert "'native/src/x/x_core.c' rose to 2" in r.stdout
