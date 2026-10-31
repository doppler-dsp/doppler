"""Pre-design prototype: operation order for a bank of hypotheses.

doppler#2683 asks Corr2D/DelayDoppler for caller control over many
hypothesis maps; doppler#2670 needs the same thing for acq's rate search.
Before the C design is frozen, this prototype settles the ORDER the
per-hypothesis work runs in, and how a fractional carrier offset is applied,
by checking each claim against the shipped pipeline:

1. **The reference is the shipped pipeline.** ``full()`` is numpy's
   statement of one hypothesis run through DelayDoppler end to end: mix,
   weight, row correlation, slow-time FFT. It is first checked against
   ``doppler.spectral.DelayDoppler`` itself, so every later comparison is a
   comparison with what ships, not with a reading of it.
2. **Both orderings equal the reference, for every hypothesis.** Rows are
   transformed once; carrier hypotheses (outer) pay the row inverses; rate
   weights (inner) pay only the slow-time FFT. Two ways to reach a carrier
   offset on the ``1/(L*nx)`` grid are compared:

   - *zero-pad then roll*: one ``L*nx``-point FFT per row, each offset a
     stride-``L`` gather, ``DFT_nx{x e^{-j2pi g n/M}}[q] = X_M[(Lq+g) mod M]``;
   - *mix per fraction*: ``L`` mixed copies of each row, one ``nx``-point
     FFT each, the whole-bin part a circular roll.

3. **The carrier is corrected within a period only.** The phase a carrier
   adds BETWEEN periods is slow-time Doppler, which the slow-time FFT
   measures; the bank leaves it alone, so a signal lands on the absolute
   Doppler row for every carrier hypothesis and a pointwise reduction across
   carrier hypotheses compares like cells.
4. **The loss bound is tight.** An offset between grid points loses at most
   the Dirichlet factor at half a grid step, which is ``sinc(1/(2L))`` to
   within 1e-4 at the sizes used; the worst case reaches it.
5. **The op counts quoted in the plan are this file's arithmetic.**

Stimuli use CONTINUOUS carrier offsets and delays: a signal placed on the
grid would hide the straddle and the droop the bank exists to handle.

The page ``docs/design/corr2d-hypotheses.md`` holds this file's tables
between ``<!-- corr2d-hypotheses:<block>:start -->`` and ``:end`` markers.
Only deterministic cells are rendered (each claim's tolerance, the loss
formula, the op counts), never a measured float error, so ``--check`` is
exact on every machine.

Usage::

    python prototype.py           # print the claims and the blocks
    python prototype.py --write   # rewrite the page's blocks
    python prototype.py --check   # exit 1 if a claim fails or a block
                                  # differs (the gate, in docs-check)

Claim 1 compares against the compiled ``DelayDoppler``, so it runs in
``src/doppler/spectral/tests/test_corr2d_hypotheses_oracle.py`` where the
extension is built; everything here is numpy and runs anywhere.

Examples
--------
>>> x, n = np.arange(4.0), np.arange(4)
>>> X = np.fft.fft(x, n=8)  # zero-pad to M = 8, so L = 2
>>> mixed = np.fft.fft(x * np.exp(-2j * np.pi * n / 8))  # offset 1/M
>>> bool(np.allclose(X[(2 * n + 1) % 8], mixed))
True
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from typing import TYPE_CHECKING

import _record_blocks as blocks
from _record_blocks import table

if TYPE_CHECKING:
    from collections.abc import Sequence

PAGE = Path(__file__).resolve().parent.parent / "corr2d-hypotheses.md"
SCRIPT = "docs/design/corr2d-hypotheses-measurements/prototype.py"

CF = NDArray[np.complex128]

#: The geometry every equality check runs at: small, so --check is quick,
#: and with ny_out > ny so the slow-time zero-pad is exercised too.
NY, NX, NY_OUT, L = 8, 64, 16, 4

#: The request's geometry, for the op counts (ny_out is assumed: #2683
#: does not state it).
REQ = {"ny": 64, "nx": 512, "ny_out": 64, "carriers": 4, "rates": 12}

#: The op counts the plan quotes, in millions of N*log2(N) units.
QUOTED = {"full": 37.75, "padded": 12.06, "mix": 11.80}


def code_of(nx: int, rng: np.random.Generator) -> CF:
    """A unit-modulus random-phase code of ``nx`` chips."""
    return np.exp(2j * np.pi * rng.random(nx))


def delayed(code: CF, d: float) -> CF:
    """``code`` delayed by ``d`` samples, circularly and band-limited, so a
    fractional delay is a real straddle, not a rounding of one."""
    k = np.fft.fftfreq(code.size) * code.size
    return np.fft.ifft(
        np.fft.fft(code) * np.exp(-2j * np.pi * k * d / code.size)
    )


def stream(code: CF, ny: int, d: float, f: float) -> CF:
    """``ny`` periods of ``code`` delayed by ``d``, carrying ``f``
    cycles/sample of carrier over the WHOLE stream (the phase runs on
    across period boundaries, as a real carrier does)."""
    n = np.arange(ny * code.size)
    return np.tile(delayed(code, d), ny) * np.exp(2j * np.pi * f * n)


def rate_weights(ny: int, count: int) -> list[CF]:
    """``count`` slow-time weight vectors: quadratic phases, the de-chirp
    shape acq's rate search applies per period (``acq_rate_table``)."""
    r = np.arange(ny) - (ny - 1) / 2
    return [
        np.exp(-1j * np.pi * a * r**2 / ny) for a in np.linspace(-1, 1, count)
    ]


def full(x: CF, code: CF, ny: int, ny_out: int, f: float, w: CF) -> CF:
    """One hypothesis through the whole pipeline: correct carrier ``f``
    within each period (the mix restarts every row), weight each row by
    ``w``, correlate each row against ``code``, slow-time FFT, center.
    Rows of the result are Doppler bins, columns delay lags."""
    nx = code.size
    n = np.arange(nx)
    y = x[: ny * nx].reshape(ny, nx) * np.exp(-2j * np.pi * f * n) * w[:, None]
    spec = np.fft.fft(y, axis=1) * np.conj(np.fft.fft(code))
    rows = np.fft.ifft(spec, axis=1)
    return np.fft.fftshift(np.fft.fft(rows, n=ny_out, axis=0), axes=0)


#: Where claim 1 (the reference is the shipped pipeline) is executed.
ORACLE_TEST = "test_corr2d_hypotheses_oracle.py"


def oracle_error() -> float:
    """Claim 1: the worst relative error between the numpy reference and
    the shipped DelayDoppler, over signed fractional carrier offsets and
    two rate weightings. Needs the extension (``make pyext``)."""
    rng = np.random.default_rng(2683)
    code = code_of(NX, rng)
    m = L * NX
    ws = rate_weights(NY, 3)
    x = stream(code, NY, 5.4, 3.37 / m)
    return max(
        rel(
            oracle(x, code, NY, NY_OUT, g / m, w),
            full(x, code, NY, NY_OUT, g / m, w),
        )
        for g in (-5, 0, 3, 7)
        for w in ws[:2]
    )


def oracle(x: CF, code: CF, ny: int, ny_out: int, f: float, w: CF) -> CF:
    """The same hypothesis through the SHIPPED DelayDoppler: mix and weight
    the input here, then let the C kernel do the rest."""
    from doppler.spectral import DelayDoppler

    nx = code.size
    n = np.arange(nx)
    y = x[: ny * nx].reshape(ny, nx) * np.exp(-2j * np.pi * f * n) * w[:, None]
    dd = DelayDoppler(code.astype(np.complex64), ny=ny, ny_out=ny_out)
    out = dd.execute(y.astype(np.complex64).ravel())
    return np.asarray(out, dtype=np.complex128).reshape(ny_out, nx)


def _slow(rows: CF, w: CF, ny_out: int) -> CF:
    """The per-rate stage: weight the correlated rows, slow-time FFT."""
    return np.fft.fftshift(
        np.fft.fft(rows * w[:, None], n=ny_out, axis=0), axes=0
    )


def ordered_padded(
    x: CF,
    code: CF,
    ny: int,
    ny_out: int,
    lf: int,
    grid: Sequence[int],
    ws: Sequence[CF],
) -> dict[tuple[int, int], CF]:
    """Every (carrier, rate) map, zero-pad-then-roll ordering.

    One ``M = lf*nx`` FFT per row, once. Carrier ``g`` (offset ``g/M``
    cycles/sample) is the gather ``X[:, (lf*q + g) mod M]``: its row
    correlations are the per-carrier cost. Each rate then costs one
    weighting and one slow-time FFT."""
    nx = code.size
    m = lf * nx
    xm = np.fft.fft(x[: ny * nx].reshape(ny, nx), n=m, axis=1)
    cref = np.conj(np.fft.fft(code))
    q = np.arange(nx)
    out = {}
    for g in grid:
        rows = np.fft.ifft(xm[:, (lf * q + g) % m] * cref, axis=1)
        for i, w in enumerate(ws):
            out[g, i] = _slow(rows, w, ny_out)
    return out


def ordered_mix(
    x: CF,
    code: CF,
    ny: int,
    ny_out: int,
    lf: int,
    grid: Sequence[int],
    ws: Sequence[CF],
) -> dict[tuple[int, int], CF]:
    """Every (carrier, rate) map, mix-per-fraction ordering.

    Carrier ``g = lf*k + j``: the fraction ``j`` is a mixed copy of the
    rows with its own ``nx``-point FFT (cached, at most ``lf`` of them),
    the whole bins ``k`` a circular roll of that spectrum."""
    nx = code.size
    n = np.arange(nx)
    rows0 = x[: ny * nx].reshape(ny, nx)
    cref = np.conj(np.fft.fft(code))
    q = np.arange(nx)
    spectra: dict[int, CF] = {}
    out = {}
    for g in grid:
        j, k = g % lf, g // lf
        if j not in spectra:
            mixed = rows0 * np.exp(-2j * np.pi * j * n / (lf * nx))
            spectra[j] = np.fft.fft(mixed, axis=1)
        rows = np.fft.ifft(spectra[j][:, (q + k) % nx] * cref, axis=1)
        for i, w in enumerate(ws):
            out[g, i] = _slow(rows, w, ny_out)
    return out


def rel(a: CF, b: CF) -> float:
    """Largest difference relative to the reference's peak."""
    return float(np.max(np.abs(a - b)) / np.max(np.abs(b)))


def dirichlet(delta: float, nx: int) -> float:
    """The coherent gain of an ``nx``-sample ramp of ``delta``
    cycles/sample, relative to no ramp: ``|sin(pi d nx) / (nx sin(pi d))|``."""
    if delta == 0:
        return 1.0
    return abs(
        math.sin(math.pi * delta * nx) / (nx * math.sin(math.pi * delta))
    )


def op_counts(
    ny: int, nx: int, ny_out: int, carriers: int, rates: int
) -> dict[str, float]:
    """FFT work per frame in millions of N*log2(N) units, by ordering.

    full: every hypothesis pays the row forward FFT, the row inverse and the
    slow-time FFT. Ordered: one forward stage per frame (one padded FFT per
    row, or one nx FFT per fraction), the row inverse per CARRIER, the
    slow-time FFT per hypothesis. ``carriers`` is taken as the L fractions
    of one whole bin, so L = carriers for the padded length."""

    def u(n: int) -> float:
        return n * math.log2(n)

    h = carriers * rates
    row = ny * u(nx)
    col = nx * u(ny_out)
    return {
        "full": h * (2 * row + col) / 1e6,
        "padded": (ny * u(carriers * nx) + carriers * row + h * col) / 1e6,
        "mix": (carriers * row + carriers * row + h * col) / 1e6,
        "column_share": h
        * col
        / (ny * u(carriers * nx) + carriers * row + h * col),
    }


def run() -> list[tuple[str, bool, str, str]]:
    """Every claim as (name, held, detail, tolerance)."""
    rng = np.random.default_rng(2683)
    code = code_of(NX, rng)
    m = L * NX
    ws = rate_weights(NY, 3)
    res: list[tuple[str, bool, str, str]] = []

    # 1. The reference is the shipped pipeline (float32 kernel vs float64).
    #    That needs the compiled extension, which the Docs job does not
    #    build, so the claim is held by a pytest test that runs where the
    #    extension is (ORACLE_TEST); its row here is the claim and its home.
    f0 = 3.37 / m
    x = stream(code, NY, 5.4, f0)
    res.append(
        (
            "the numpy reference equals the shipped `DelayDoppler`",
            True,
            f"held by {ORACLE_TEST}",
            f"2e-5 (float32 kernel), `{ORACLE_TEST}`",
        )
    )

    # 2. Both orderings == the reference, every hypothesis, signed offsets
    #    spanning whole bins and fractions.
    grid = list(range(-2 * L - 1, 2 * L + 2))
    pad = ordered_padded(x, code, NY, NY_OUT, L, grid, ws)
    mix = ordered_mix(x, code, NY, NY_OUT, L, grid, ws)
    ref = {
        (g, i): full(x, code, NY, NY_OUT, g / m, w)
        for g in grid
        for i, w in enumerate(ws)
    }
    ep = max(rel(pad[h], ref[h]) for h in ref)
    em = max(rel(mix[h], ref[h]) for h in ref)
    res.append(
        (
            f"zero-pad then roll equals the reference, all {len(ref)} maps",
            ep < 1e-12,
            f"max rel err {ep:.1e}",
            "1e-12",
        )
    )
    res.append(
        (
            f"mix per fraction equals the reference, all {len(ref)} maps",
            em < 1e-12,
            f"max rel err {em:.1e}",
            "1e-12",
        )
    )

    # 3. Within-period correction only: for continuous offsets the peak sits
    #    on the ABSOLUTE Doppler row (all of f0's phase between periods),
    #    the same row for every carrier hypothesis.
    one = np.ones(NY)
    worst_row = 0
    for f in rng.uniform(-3, 3, 6) / NX:
        d = float(rng.uniform(0, NX))
        xs = stream(code, NY, d, f)
        nu = (f * NX + 0.5) % 1 - 0.5  # cycles/period, wrapped
        want_row = NY_OUT // 2 + nu * NY_OUT
        for g in (round(f * m) - 1, round(f * m), round(f * m) + 1):
            mp = full(xs, code, NY, NY_OUT, g / m, one)
            r, c = np.unravel_index(np.argmax(np.abs(mp)), mp.shape)
            dr = abs((r - want_row + NY_OUT / 2) % NY_OUT - NY_OUT / 2)
            dc = abs((c - d + NX / 2) % NX - NX / 2)
            worst_row = max(worst_row, dr)
            if dc > 1:
                worst_row = 99
    res.append(
        (
            "the peak sits on the absolute Doppler row under every "
            "carrier hypothesis",
            worst_row <= 0.5,
            f"worst row error {worst_row:.2f} bins",
            "half a Doppler bin; delay within one lag",
        )
    )

    # 4. The loss bound, at its worst case and inside it. Integer delay and
    #    a unit-modulus code make the within-period loss exactly Dirichlet.
    lows = []
    for frac in (0.5, 0.31, 0.12):  # in grid steps from the nearest point
        f = (11 + frac) / m
        xs = stream(code, NY, 9.0, f)
        got = np.max(np.abs(full(xs, code, NY, NY_OUT, 11 / m, one)))
        best = np.max(np.abs(full(xs, code, NY, NY_OUT, f, one)))
        lows.append((frac, got / best, dirichlet(frac / m, NX)))
    worst = lows[0]
    held = all(abs(r - dl) < 1e-9 for _, r, dl in lows)
    bound = float(np.sinc(1 / (2 * L)))
    res.append(
        (
            "the loss between grid points is the Dirichlet factor, "
            "worst at half a step",
            held and abs(worst[1] - bound) < 1e-3,
            f"half step {20 * math.log10(worst[1]):.2f} dB, sinc(1/2L) "
            f"{20 * math.log10(bound):.2f} dB",
            "1e-9 to Dirichlet; 1e-3 to sinc(1/(2L))",
        )
    )

    # 5. The plan's op counts.
    oc = op_counts(**REQ)
    held = all(abs(round(oc[k], 2) - v) < 1e-9 for k, v in QUOTED.items())
    res.append(
        (
            "the op counts are the plan's",
            held and round(oc["column_share"], 2) == 0.78,
            f"full {oc['full']:.2f}M, padded {oc['padded']:.2f}M, "
            f"mix {oc['mix']:.2f}M, column share {oc['column_share']:.0%}",
            "exact, to two places",
        )
    )
    return res


def claims_block(res: list[tuple[str, bool, str, str]]) -> str:
    """The claims and the tolerance each is held to."""
    return table(
        ["claim", "held to"], [[name, tol] for name, _, _, tol in res]
    )


def loss_block() -> str:
    """Worst within-period loss per grid factor L, at the plan's nx."""
    nx = REQ["nx"]
    rows = []
    for lf in (1, 2, 4, 8):
        step = "$1/n_x$" if lf == 1 else f"$1/({lf}\\,n_x)$"
        db = 20 * math.log10(dirichlet(1 / (2 * lf * nx), nx))
        rows.append([f"{lf}", step, f"{-db:.2f} dB"])
    return table(["$L$", "grid step (cycles/sample)", "worst loss"], rows)


def ops_block() -> str:
    """The op counts per stage at the request's geometry, by ordering."""
    g = REQ
    h = g["carriers"] * g["rates"]

    def u(n: int) -> float:
        return n * math.log2(n)

    row = g["ny"] * u(g["nx"]) / 1e6
    col = g["nx"] * u(g["ny_out"]) / 1e6
    pad_fwd = g["ny"] * u(g["carriers"] * g["nx"]) / 1e6
    c = g["carriers"]
    oc = op_counts(**g)
    return table(
        [
            "stage",
            "every hypothesis, full pipeline",
            "ordered, zero-pad then roll",
            "ordered, mix per fraction",
        ],
        [
            [
                "row forward FFT",
                f"{h * row:.2f} ({h} x)",
                f"{pad_fwd:.2f} (one per row)",
                f"{c * row:.2f} ({c} x)",
            ],
            [
                "row inverse FFT",
                f"{h * row:.2f} ({h} x)",
                f"{c * row:.2f} ({c} x)",
                f"{c * row:.2f} ({c} x)",
            ],
            [
                "slow-time FFT",
                f"{h * col:.2f} ({h} x)",
                f"{h * col:.2f} ({h} x)",
                f"{h * col:.2f} ({h} x)",
            ],
            [
                "total",
                f"{oc['full']:.2f}",
                f"{oc['padded']:.2f}",
                f"{oc['mix']:.2f}",
            ],
        ],
    )


def main(argv: Sequence[str] | None = None) -> int:
    """Run every claim; refuse on a failure; then print, write or check
    the page's blocks."""
    res = run()
    for name, held, detail, _ in res:
        print(f"{'ok  ' if held else 'FAIL'} {name}: {detail}")
    if not all(held for _, held, _, _ in res):
        print("corr2d-hypotheses: a claim failed; the page is not rendered")
        return 1
    return blocks.main(
        PAGE,
        "corr2d-hypotheses",
        "corr2d-hypotheses",
        SCRIPT,
        lambda: {
            "claims": claims_block(res),
            "loss": loss_block(),
            "ops": ops_block(),
        },
        __doc__ or "",
        argv,
    )


if __name__ == "__main__":
    sys.exit(main())
