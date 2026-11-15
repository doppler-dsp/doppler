"""The stimulus-sources gate's C noise-level signature, over a seeded tree.

`scripts/check_stimulus_sources.py` refuses a private stimulus convention.
Its `level` marker gained a C spelling from doppler#689: `dp_cgauss` is
already unit power (``E|z|^2 = 1``, 0.5 per component), and nine DSSS noise
sites scaled it by a further ``/ sqrt (2.0)`` per component, so each capture
carried 3.01 dB less noise than its C/N0 label said.

Each case seeds a fake tree and points ``--root`` at it, because a gate that
can only run against the real tree cannot be sabotaged. The cases past the
defect itself guard against an eager gate: a draw scaled by a literal states
its own level, ``M_SQRT1_2`` mixing two values with no draw is arithmetic,
and the library under ``native/src`` is the sanctioned home. The allowlist
cases pin that the ratchet counts SITES, not distinct line texts: three
identical draws in one file are three entries, and a fourth goes red.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_stimulus_sources.py"
ALLOW = "scripts/.stimulus-sources-allow"
TEST = "native/tests/a/test_a_core.c"

#: The defect as found in native/tests/dp_dsss_test.h.
DRAW = "x[i] = (float _Complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);"
#: Its allowlist key: marker, path, the STATEMENT (no `;`) with whitespace
#: normalised.
KEY = f"level::{TEST}::{DRAW.rstrip(';')}"


def _run(tmp_path: Path, body: str, rel: str = TEST, allow: str = ""):
    """Seed one file at ``rel`` (and an allowlist), run the gate over it."""
    p = tmp_path / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(body, encoding="utf-8")
    if allow:
        a = tmp_path / ALLOW
        a.parent.mkdir(parents=True, exist_ok=True)
        a.write_text(allow, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


def _fn(*lines: str) -> str:
    """Wrap statements in a C function body, GNU-indented as the tree is."""
    return "static void\nf (void)\n{\n" + "".join(f"  {s}\n" for s in lines)


def test_a_clean_tree_passes(tmp_path: Path) -> None:
    r = _run(tmp_path, _fn("double a = 1.0;"))
    assert r.returncode == 0, r.stdout + r.stderr
    assert "level 0" in r.stdout


def test_the_689_draw_fails_and_names_the_call_instead(tmp_path: Path):
    """The site as found, refused with the primitive to call instead."""
    r = _run(tmp_path, _fn(DRAW))
    assert r.returncode == 1, r.stdout
    assert f"[level] {TEST}:4" in r.stdout
    assert "dp_awgn_amplitude_for_snr" in r.stdout
    assert "snr/snr_mode" in r.stdout
    assert "doppler#689" in r.stdout


def test_every_spelling_of_the_second_sqrt2_fails(tmp_path: Path) -> None:
    """Every spelling of 1/sqrt(2), in any order, is one defect.

    The adversary review of #3086 found each of these passing the first,
    line-wise cut of the gate: float spellings, ``M_SQRT2`` as a divisor, a
    cast between the factor and the draw, the draw written first, the
    factor trailing, sqrt(1/2) as a factor, and the constant written out.
    """
    for line in (
        "x[i] += (float complex) (sd / sqrt (2)) * dp_cgauss (&nst);",
        "y = sigma/sqrt(2.0)*dp_cgauss(&st);",
        "y = (float _Complex) (M_SQRT1_2 * sigma) * dp_cgauss (&st);",
        "y = (sigma / sqrtf (2.0f)) * dp_cgauss (&st);",
        "y = (sigma / sqrt (2.0f)) * dp_cgauss (&st);",
        "y = (sigma / M_SQRT2) * dp_cgauss (&st);",
        "y = (sigma / (float) sqrt (2)) * dp_cgauss (&st);",
        "y = (sigma / sqrt (2.0)) * (double _Complex) dp_cgauss (&st);",
        "y = dp_cgauss (&st) * sigma / sqrt (2.0);",
        "y = sigma * dp_cgauss (&st) / sqrt (2.0);",
        "y = sigma * sqrt (0.5) * dp_cgauss (&st);",
        "y = 0.70710678 * sigma * dp_cgauss (&st);",
        "y = sigma / 1.41421356 * dp_cgauss (&st);",
        # The re-review's spellings: doubled and multi-word casts, a cast
        # inside the sqrt, long-double and short constants, M_SQRT2 / 2.
        "y = (sigma / ((double)) sqrt (2)) * dp_cgauss (&st);",
        "y = (sigma / (long double) sqrtl (2.0L)) * dp_cgauss (&st);",
        "y = (sigma / sqrt ((double) 2)) * dp_cgauss (&st);",
        "y = sigma / 1.4142 * dp_cgauss (&st);",
        "y = 0.707f * sigma * dp_cgauss (&st);",
        "y = M_SQRT2 / 2.0 * sigma * dp_cgauss (&st);",
        "y = dp_cgauss (&st) * sigma * M_SQRT1_2;",
        # A unary sign or an exponent's sign is not a sum: the confirm
        # review found both escaping the first term split.
        "y = sigma / sqrt (2.0) * -dp_cgauss (&st);",
        "y = sigma / sqrt (2.0) * 1e-3 * dp_cgauss (&st);",
        "y = sigma / sqrt (2.0) * 1.5E+2 * dp_cgauss (&st);",
        "y = -sigma / sqrt (2.0) * dp_cgauss (&s->st);",
        # A sign after a cast is unary too.
        "y = sigma / sqrt (2.0) * (float _Complex) -dp_cgauss (&st);",
        "y = sigma / sqrt (2.0) * (double) -1.0 * dp_cgauss (&st);",
    ):
        r = _run(tmp_path, _fn(line))
        assert r.returncode == 1, line + "\n" + r.stdout
        assert "[level]" in r.stdout, line


def test_a_continuation_line_is_still_the_draw(tmp_path: Path) -> None:
    """test_dsss_receiver_core.c:316 breaks the statement before the `=`."""
    body = _fn(
        "x[i]",
        "    = (float _Complex) (sigma / sqrt (2.0)) * dp_cgauss (&nst);",
    )
    r = _run(tmp_path, body)
    assert r.returncode == 1, r.stdout


def test_a_wrap_before_the_product_is_still_the_draw(tmp_path: Path):
    """GNU clang-format breaks a long product BEFORE its ``*``, so the
    continuation line starts with ``*`` -- which a line-wise scan skipped as
    comment prose, and then called the allowlist entry stale."""
    body = _fn(
        "x[i] = (float _Complex) (sigma / sqrt (2.0))",
        "       * dp_cgauss (&st);",
    )
    r = _run(tmp_path, body)
    assert r.returncode == 1, r.stdout
    assert f"[level] {TEST}:4" in r.stdout


def test_a_string_literal_does_not_hide_the_draw(tmp_path: Path):
    """A ``"/*"`` or a URL in a string earlier on the line opened a
    comment in the first statement cut and swallowed the draw."""
    for pre in ('puts ("/*");', 'puts ("http://x");', "char c = '/';"):
        r = _run(tmp_path, _fn(pre + " " + DRAW))
        assert r.returncode == 1, pre + "\n" + r.stdout


def test_a_macro_is_read_whole(tmp_path: Path) -> None:
    """A ``#define`` (with a continuation) is one statement, not skipped."""
    body = (
        "#define NOISE(s, st) \\\n  ((s) / sqrt (2.0) * dp_cgauss (st))\n"
    ) + _fn("double a = 1.0;")
    r = _run(tmp_path, body)
    assert r.returncode == 1, r.stdout
    assert f"[level] {TEST}:1" in r.stdout


def test_a_benchmark_or_an_example_is_in_scope(tmp_path: Path) -> None:
    for rel in ("native/benchmarks/b.c", "native/examples/e.c"):
        r = _run(tmp_path / rel.split("/")[1], _fn(DRAW), rel=rel)
        assert r.returncode == 1, rel + "\n" + r.stdout


def test_what_is_not_the_defect_passes(tmp_path: Path) -> None:
    """A level the line states, arithmetic with no draw, a draw scaled on
    another statement, prose, and the library itself."""
    cases = [
        (_fn("x[i] = 0.3f * dp_cgauss (&st);"), TEST),
        (_fn("x[i] = (float _Complex) sigma * dp_cgauss (&st);"), TEST),
        (_fn("double c = M_SQRT1_2 * b;"), TEST),
        (_fn("double c = a / sqrt (2.0) * b;"), TEST),
        # sqrt(2) as a MULTIPLIER is a different, louder scaling with real
        # uses: test_detection_core.c builds a real Gaussian this way.
        (
            _fn("v = M_SQRT2 * creal ((double _Complex) dp_cgauss (&st));"),
            TEST,
        ),
        (_fn("v = sqrt (2.0) * creal (dp_cgauss (&st));"), TEST),
        # A unit-power QPSK symbol PLUS correctly-scaled noise: the root-2
        # and the draw are in different product terms.
        (
            _fn("y = M_SQRT1_2 * (b0 + I * b1) + sigma * dp_cgauss (&st);"),
            TEST,
        ),
        (
            _fn("y = (si + I * sq) / sqrt (2.0) + sigma * dp_cgauss (&st);"),
            TEST,
        ),
        (_fn("y = s->a / sqrt (2.0) - s->sigma * dp_cgauss (&s->st);"), TEST),
        # A parenthesised VALUE (not a type) is an operand: this is a sum.
        (_fn("y = (a) / sqrt (2.0) - (sigma) * dp_cgauss (&st);"), TEST),
        # A string mentioning the defect is not code.
        (_fn('puts ("sigma / sqrt (2.0) * dp_cgauss (&st)");'), TEST),
        (
            _fn(
                "double s = sigma / sqrt (2.0);", "x = s; y = dp_cgauss (&st);"
            ),
            TEST,
        ),
        (_fn("/* sigma / sqrt (2.0) * dp_cgauss (&st); */"), TEST),
        (_fn(DRAW), "native/src/awgn/awgn_core.c"),
        (_fn(DRAW), "native/tests/a/test_a.py"),
    ]
    for i, (body, rel) in enumerate(cases):
        r = _run(tmp_path / str(i), body, rel=rel)
        assert r.returncode == 0, f"case {i} ({rel})\n" + r.stdout


def test_an_allowlisted_site_passes(tmp_path: Path) -> None:
    r = _run(tmp_path, _fn(DRAW), allow=f"{KEY} | debt, #689\n")
    assert r.returncode == 0, r.stdout + r.stderr
    assert "level 1" in r.stdout


def test_the_ratchet_counts_sites_not_line_texts(tmp_path: Path) -> None:
    """A second identical draw in the same file does not ride in on the
    first one's entry, and two entries cover two sites exactly."""
    one = f"{KEY} | debt, #689\n"
    r = _run(tmp_path, _fn(DRAW, DRAW), allow=one)
    assert r.returncode == 1, r.stdout
    assert f"[level] {TEST}:5" in r.stdout
    r = _run(tmp_path, _fn(DRAW, DRAW), allow=one * 2)
    assert r.returncode == 0, r.stdout


def test_an_entry_for_a_site_that_went_away_is_stale(tmp_path: Path) -> None:
    r = _run(tmp_path, _fn(DRAW), allow=f"{KEY} | debt, #689\n" * 2)
    assert r.returncode == 1, r.stdout
    assert "gone stale" in r.stdout
    assert KEY in r.stdout
