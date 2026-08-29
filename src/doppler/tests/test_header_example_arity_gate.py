"""The header-example arity gate, exercised over a seeded tree.

`scripts/check_header_example_arity.py` exists because a header's example
block is compiled by NOTHING. `docs/**` fences are built `-Werror` against
`libdoppler.a`; a header's block is rendered by doxygen, published to
`docs/c-api/**`, and — for a constructor — transplanted by jm into the
`.pyi` a Python user reads. Not one of those paths type-checks it, so
`mpsk_receiver_create()`'s example passed SIXTEEN arguments to a fifteen-
parameter function and said so to nobody (doppler#1082).

The cases below seed a fake `native/inc/`, because a gate that can only be
tested against the real tree cannot be sabotaged: you would have to break
doppler to check it, and nobody does that twice.

Two of them are about what the gate must NOT say. A Python doctest carries
the *binding's* arity, not the C function's, and this tree keeps its Python
examples in exactly these blocks — so judging one against the C declaration
would bury the real findings in noise. And a variadic declaration cannot be
judged at all.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_header_example_arity.py"

_GOOD = """\
/**
 * @brief A widget.
 *
 * @code
 * widget_t *w = widget_create (1, 2, 3);
 * widget_destroy (w);
 * @endcode
 */
widget_t *widget_create (int a, int b, int c);
void widget_destroy (widget_t *w);
"""

#: The shape that shipped: the example was written, then a parameter was
#: added to the declaration and the example stayed as it was.
_STALE = """\
/**
 * @code
 * widget_t *w = widget_create (1, 2, 3);
 * @endcode
 */
widget_t *widget_create (int a, int b, int c, int d);
"""

#: A Python doctest. `widget_create(1)` is the BINDING's arity.
_PYTHON = """\
/**
 * @code
 * >>> from doppler.widget import widget_create
 * >>> widget_create(1)
 * <widget>
 * @endcode
 */
widget_t *widget_create (int a, int b, int c);
"""

#: Nested calls put commas below the top level; counting those is the whole
#: way to get this wrong.
_NESTED = """\
/**
 * @code
 * widget_t *w = widget_create (pick (1, 2), other (3, 4, 5), 6);
 * @endcode
 */
widget_t *widget_create (int a, int b, int c);
"""

_VARIADIC = """\
/**
 * @code
 * widget_log ("x", 1, 2, 3, 4);
 * @endcode
 */
int widget_log (const char *fmt, ...);
"""


def _seed(tmp_path: Path, headers: dict[str, str], ratchet: str = "") -> None:
    inc = tmp_path / "native" / "inc"
    for rel, body in headers.items():
        p = inc / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    inc.mkdir(parents=True, exist_ok=True)
    (inc / ".example-arity-ratchet").write_text(ratchet, encoding="utf-8")


def _run(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


def test_a_matching_example_passes(tmp_path: Path) -> None:
    _seed(tmp_path, {"widget/widget.h": _GOOD})
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "OK" in r.stdout


def test_a_stale_example_fails_and_names_both_counts(tmp_path: Path) -> None:
    """The defect that shipped: signature grew, example did not."""
    _seed(tmp_path, {"widget/widget.h": _STALE})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "widget_create()" in r.stdout
    # Both numbers, because "wrong" alone costs a round trip to act on.
    assert "3 argument(s)" in r.stdout
    assert "declared with 4" in r.stdout


def test_a_python_doctest_is_not_judged(tmp_path: Path) -> None:
    """A `>>>` block carries the binding's arity, not the C function's."""
    _seed(tmp_path, {"widget/widget.h": _PYTHON})
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_nested_calls_do_not_inflate_the_count(tmp_path: Path) -> None:
    _seed(tmp_path, {"widget/widget.h": _NESTED})
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_a_variadic_declaration_is_not_judged(tmp_path: Path) -> None:
    _seed(tmp_path, {"widget/widget.h": _VARIADIC})
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_the_ratchet_waives_a_known_one(tmp_path: Path) -> None:
    _seed(
        tmp_path,
        {"widget/widget.h": _STALE},
        ratchet="native/inc/widget/widget.h widget_create\n",
    )
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "1 ratcheted" in r.stdout


def test_the_ratchet_may_only_shrink(tmp_path: Path) -> None:
    """A waiver kept after its example was fixed hides the NEXT regression.

    This is the direction that shipped broken elsewhere in this repo — a
    ratchet with no staleness check keeps covering a file that improved.
    """
    _seed(
        tmp_path,
        {"widget/widget.h": _GOOD},
        ratchet="native/inc/widget/widget.h widget_create\n",
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "no longer" in r.stdout
    assert "may only shrink" in r.stdout


def test_a_waiver_does_not_cover_a_different_function(tmp_path: Path) -> None:
    """Keyed on the function, so one waiver cannot silence a second defect."""
    two = (
        _STALE
        + """
/**
 * @code
 * gadget_make (1);
 * @endcode
 */
int gadget_make (int a, int b);
"""
    )
    _seed(
        tmp_path,
        {"widget/widget.h": two},
        ratchet="native/inc/widget/widget.h widget_create\n",
    )
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "gadget_make()" in r.stdout
    assert "widget_create()" not in r.stdout
