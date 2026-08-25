"""Certify `Corr2D` — the 2-D correlator every DSSS acquisition sits on.

Run:  python -m doppler.spectral.tests.validation.corr2d.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

`Corr2D` is the correlation surface `Acquisition` searches: one FFT2, a
pointwise multiply against a precomputed reference spectrum, a coherent
integrate-and-dump, and an inverse that may be LARGER than the forward, which
band-limits-interpolates the surface onto a finer grid for sub-bin peak
resolution. Two of those — the single-row fast path and the interpolated
inverse — are optimisations that must be *exactly* equivalent to the thing
they replace, and that is most of what this report measures.

**Measured against an external truth wherever possible.** A brute-force
circular correlation needs no FFT, no window convention and no scaling
argument, so it is what the transform path is checked against rather than the
transform path being checked against itself. Where a claim is only reachable
from C — `set_ref`, the `fast_path` flag, the native path's NULL scratch
pointers — the report says so and names the C section that covers it.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.spectral import Corr2D
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

SEED = 20260824


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    brute_rows: list[list[str]] = field(default_factory=list)
    brute_worst: float = 0.0
    dump_schedule_ok: bool = False
    dump_rows: list[list[str]] = field(default_factory=list)
    dwell_is_sum: float = 0.0
    fast_matches_general: float = 0.0
    interp_rows: list[list[str]] = field(default_factory=list)
    interp_worst_bins: float = 0.0
    native_grid_worst: float = 0.0
    real_stays_real: float = 0.0
    real_peak: float = 0.0
    accuracy_rows: list[list[str]] = field(default_factory=list)
    accuracy_worst: float = 0.0
    state_exact: bool = False
    state_rejects: bool = False
    reset_clears: bool = False
    trunc_ok: bool = False
    n_out_matches: bool = False
    identity_err: float = 0.0
    peak_preserved: float = 0.0
    reset_keeps_config: bool = False
    rejects_bad_args: bool = False


def _rng(tag: int) -> np.random.Generator:
    return np.random.default_rng(SEED + tag)


def _cplx(r: np.random.Generator, *shape: int) -> np.ndarray:
    return (r.standard_normal(shape) + 1j * r.standard_normal(shape)).astype(
        np.complex64
    )


def _brute_corr2d(x: np.ndarray, h: np.ndarray) -> np.ndarray:
    """Circular 2-D cross-correlation, straight from the definition.

    `R[i,j] = sum_{p,q} x[p,q] * conj(h[(p-i) mod ny, (q-j) mod nx])`.
    No transform, no scaling convention, no window — this is the external
    truth the FFT path is measured against, and it is deliberately the slow
    O(n^2) form so that nothing it shares with the implementation can hide a
    defect in both.
    """
    ny, nx = x.shape
    out = np.zeros((ny, nx), dtype=np.complex128)
    xh = x.astype(np.complex128)
    hh = h.astype(np.complex128)
    for i in range(ny):
        for j in range(nx):
            out[i, j] = np.sum(xh * np.conj(np.roll(hh, (i, j), axis=(0, 1))))
    return out


def _band_limited_impulse(nx: int, frac: float) -> np.ndarray:
    """A unit impulse at a FRACTIONAL position, exactly band-limited.

    Built as the inverse DFT of a pure linear phase over SIGNED frequencies
    (0, +1, -1, +2, -2, ...). Sweeping `u = 0..nx-1` instead treats the upper
    bins as high positive frequencies rather than the negative ones they are,
    which is not a spectrum any correlation surface has — and produces two
    equal peaks straddling a null at the true position, which reads exactly
    like a broken interpolator. See §3 F3.
    """
    j = np.arange(nx)[:, None]
    k = np.arange(nx)[None, :]
    u = np.where(k == 0, 0, np.where(k % 2, (k + 1) // 2, -(k // 2)))
    return (np.exp(2j * np.pi * u * (j - frac) / nx).sum(axis=1) / nx).astype(
        np.complex64
    )


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — a correlation surface with two optimisations")
    R.md()
    R.md(
        "`Corr2D` correlates a 2-D frame against a precomputed reference "
        "spectrum and coherently integrates `dwell` frames before dumping. "
        "It is what `Acquisition` searches: the fast axis is the code "
        "matched filter, the slow axis the Doppler search."
    )
    R.md()
    R.md("Neither of these is restated here:")
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/design/corr2d-interpolated-inverse.md`]"
                "(../../../../../../docs/design/corr2d-interpolated-inverse.md)",
                "why the inverse may be larger than the forward, the "
                "zero-pad maths, and the single-row fast path",
            ],
            [
                "`native/inc/corr2d/corr2d_core.h`",
                "the contract per function — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`. The C test is "
        "`test_corr2d_core.c`. Three rows were **absent or vacuous** when "
        "this certification began; three more are unreachable from Python "
        "and are certified in C instead."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "`R = IFFT2(FFT2(x)·conj(FFT2(h)))/(ny·nx)`",
                "C, vs a brute-force reference, both paths",
                "§2.1",
            ],
            [
                "coherently sums `dwell` frames, then dumps",
                "C, at dwell=2",
                "§2.2",
            ],
            [
                "single-row reference takes an exact fast path",
                "C — the `fast_path` flag is **C-ONLY**",
                "§2.3",
            ],
            [
                "a larger inverse zero-pads to the Dirichlet interpolation",
                "**was integer shifts only** — now fractional",
                "§2.4",
            ],
            [
                "the even-`n` Nyquist bin is split (matches "
                "`scipy.signal.resample`)",
                "**was nothing** — deleting the split left the suite green",
                "§2.5",
            ],
            [
                "`reset` zeroes the accumulator and the counter",
                "**was vacuous** — asserted `count==0` on a fresh object",
                "§2.7",
            ],
            [
                "native output is bit-exact and allocates no extra buffers",
                "**was nothing** — C-ONLY (NULL scratch pointers)",
                "C §native-scratch",
            ],
            [
                "`set_ref` rejects a non-single-row ref on a fast-path object",
                "C — **C-ONLY**, `set_ref` has no binding",
                "C §set_ref",
            ],
            [
                "`nthreads` is accepted and ignored",
                "C, as a forward guard",
                "F4",
            ],
            [
                "`execute_max_out()` is `ny*nx`; emission stops at `max_out`",
                "C + Python",
                "§2.8",
            ],
            ["the state triplet round-trips", "C + Python", "§2.7"],
        ],
    )


# ── 2. characterisation ───────────────────────────────────────────────


def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md("Measured behaviour. No verdicts — those are §3.")
    R.md()
    _sec_brute(d)
    _sec_dump(d)
    _sec_fastpath(d)
    _sec_interp(d)
    _sec_real(d)
    _sec_accuracy(d)
    _sec_lifecycle(d)
    _sec_bounds(d)
    return d


def _sec_brute(d: Data) -> None:
    R.md("### 2.1 The correlation theorem, against a brute-force truth")
    R.md()
    R.md(
        "A brute-force circular correlation shares no code, no transform and "
        "no scaling convention with the implementation, so agreement is "
        "evidence rather than self-consistency. Both reference shapes are "
        "swept: a single-row reference (which takes the fast path) and a "
        "genuinely 2-D one (which does not)."
    )
    R.md()
    rows, csv = [], []
    worst = 0.0
    for tag, (ny, nx) in enumerate([(4, 4), (5, 7), (8, 8), (3, 16)]):
        r = _rng(tag)
        x = _cplx(r, ny, nx)
        for shape in ("single-row", "full 2-D"):
            ref = np.zeros((ny, nx), dtype=np.complex64)
            if shape == "single-row":
                ref[0] = _cplx(r, nx)
            else:
                ref = _cplx(r, ny, nx)
            c = Corr2D(ref=ref, dwell=1)
            got = np.asarray(c.execute(x.ravel())).reshape(ny, nx)
            want = _brute_corr2d(x, ref)
            err = float(np.max(np.abs(got - want)) / np.max(np.abs(want)))
            worst = max(worst, err)
            rows.append([f"{ny}x{nx}", shape, f"{err:.2e}"])
            csv.append([ny, nx, 0 if shape == "single-row" else 1, err])
    R.table(["shape", "reference", "max relative error"], rows)
    _csv(HERE / "data" / "brute_force.csv", "ny,nx,full_2d,rel_err", csv)
    d.brute_rows = rows
    d.brute_worst = worst
    R.md(
        f"Worst **{worst:.2e}** over eight cells, which is float32 transform "
        f"noise and not a modelling gap. Raw sweep: `data/brute_force.csv`."
    )
    R.md()


def _sec_dump(d: Data) -> None:
    R.md("### 2.2 Integrate-and-dump emits on the dwell-th call, and sums")
    R.md()
    rows = []
    ok = True
    ny, nx = 4, 4
    r = _rng(100)
    ref = np.zeros((ny, nx), dtype=np.complex64)
    ref[0] = _cplx(r, nx)
    for dwell in (1, 2, 3, 5):
        c = Corr2D(ref=ref, dwell=dwell)
        frames = [_cplx(r, ny, nx) for _ in range(dwell)]
        emitted = []
        for _k, f in enumerate(frames):
            got = c.execute(f.ravel())
            emitted.append(got is not None and len(np.asarray(got)) > 0)
        ok &= emitted == [False] * (dwell - 1) + [True]
        rows.append(
            [
                str(dwell),
                "".join("D" if e else "." for e in emitted),
                "yes" if emitted[-1] else "**no**",
            ]
        )
    R.table(
        ["dwell", "call pattern (. = held, D = dump)", "dumped last"], rows
    )
    d.dump_rows = rows
    d.dump_schedule_ok = ok
    R.md(
        "The dump is the **coherent sum**, not the last frame: correlating "
        "`dwell` copies of one frame and correlating it once at `dwell=1` "
        "differ by exactly the integer factor."
    )
    R.md()
    c1 = Corr2D(ref=ref, dwell=1)
    c3 = Corr2D(ref=ref, dwell=3)
    f = _cplx(_rng(101), ny, nx)
    one = np.asarray(c1.execute(f.ravel()))
    for _ in range(2):
        c3.execute(f.ravel())
    three = np.asarray(c3.execute(f.ravel()))
    d.dwell_is_sum = float(
        np.max(np.abs(three - 3.0 * one)) / np.max(np.abs(one))
    )
    R.md(
        f"Measured: `|dump(dwell=3) - 3·dump(dwell=1)|` is "
        f"**{d.dwell_is_sum:.2e}** relative — a sum, not an average and not "
        f"a replacement."
    )
    R.md()


def _sec_fastpath(d: Data) -> None:
    R.md("### 2.3 The single-row fast path is exact (its flag is C-ONLY)")
    R.md()
    R.md(
        "A reference that is nonzero only in row 0 makes `ref_spec[u,v]` "
        "independent of `u`, so the row axis of the 2-D transform pair "
        "cancels identically and `ny` independent 1-D transforms compute the "
        "same surface. The object selects that path itself; **the "
        "`fast_path` flag has no binding**, so which path ran is asserted in "
        "`test_corr2d_core.c` and only the RESULT is checkable here."
    )
    R.md()
    worst = 0.0
    rows = []
    for tag, (ny, nx) in enumerate([(4, 8), (5, 7), (8, 16)]):
        r = _rng(200 + tag)
        x = _cplx(r, ny, nx)
        row = _cplx(r, nx)
        ref_fast = np.zeros((ny, nx), dtype=np.complex64)
        ref_fast[0] = row
        # The general path, forced: a reference with the same row 0 and an
        # explicitly zero row elsewhere is still single-row, so instead the
        # comparison is against the brute-force truth for the same reference.
        got = np.asarray(
            Corr2D(ref=ref_fast, dwell=1).execute(x.ravel())
        ).reshape(ny, nx)
        want = _brute_corr2d(x, ref_fast)
        err = float(np.max(np.abs(got - want)) / np.max(np.abs(want)))
        worst = max(worst, err)
        rows.append([f"{ny}x{nx}", f"{err:.2e}"])
    R.table(["shape", "fast path vs brute force"], rows)
    d.fast_matches_general = worst
    R.md(
        f"Worst **{worst:.2e}**. Measured against the external truth rather "
        f"than against the general path: two implementations agreeing is "
        f"blind to any defect they share, and the whole claim is that these "
        f"two compute the same thing."
    )
    R.md()


def _sec_interp(d: Data) -> None:
    R.md("### 2.4 A larger inverse resolves a FRACTIONAL peak")
    R.md()
    R.md(
        "`nx_out > nx` zero-pads the cross-spectrum before the inverse, "
        "giving the band-limited interpolation of the same surface on a "
        "finer grid. The claim is sub-bin resolution, so it is measured at "
        "sub-bin offsets — at integer shifts the native grid already lands "
        "on the answer and interpolation proves nothing."
    )
    R.md()
    rows, csv = [], []
    worst = 0.0
    nat_worst = 0.0
    up = 8
    for nx in (16, 15):
        ref = np.zeros((1, nx), dtype=np.complex64)
        ref[0, 0] = 1.0
        for frac in (3.0, 3.25, 3.5, 3.75):
            x = _band_limited_impulse(nx, frac).reshape(1, nx)
            fine = np.asarray(
                Corr2D(ref=ref, dwell=1, nx_out=nx * up).execute(x.ravel())
            )
            got = float(np.argmax(np.abs(fine))) / up
            err = abs(got - frac)
            worst = max(worst, err)
            nat = np.asarray(Corr2D(ref=ref, dwell=1).execute(x.ravel()))
            nat_err = abs(float(np.argmax(np.abs(nat))) - frac)
            nat_worst = max(nat_worst, nat_err)
            rows.append(
                [
                    f"{nx}",
                    f"{frac:g}",
                    f"{got:.3f}",
                    f"{err:.3f}",
                    f"{nat_err:.2f}",
                ]
            )
            csv.append([nx, frac, got, err, nat_err])
    R.table(
        [
            "nx",
            "true offset (bins)",
            "interpolated peak",
            "error",
            "native-grid error",
        ],
        rows,
    )
    _csv(
        HERE / "data" / "interpolation.csv",
        "nx,frac,peak,err,native_err",
        csv,
    )
    d.interp_rows = rows
    d.interp_worst_bins = worst
    d.native_grid_worst = nat_worst
    R.md(
        f"The interpolated peak lands within **{worst:.3f}** of a bin at "
        f"every offset, on both parities of `nx`, while the native grid is "
        f"off by up to {nat_worst:.2f} — which is what a half-bin offset "
        f"costs when the grid cannot represent it. Raw sweep: "
        f"`data/interpolation.csv`."
    )
    R.md()


def _sec_real(d: Data) -> None:
    R.md("### 2.5 The interpolation keeps a real correlation real")
    R.md()
    R.md(
        "The zero-pad splits the even-`n` Nyquist bin, and that split is "
        "invisible to a peak-location test — deleting it moves no peak. The "
        "invariant that sees it needs no reference implementation: a real "
        "input against a real reference gives a real correlation, whose "
        "spectrum is conjugate-symmetric, and the band-limited "
        "interpolation of a real sequence is real. An unsplit Nyquist bin "
        "turns `cos(pi t)` into `exp(i pi t)` — identical on the sample grid "
        "and complex between."
    )
    R.md()
    nx, up = 16, 4
    ref = np.zeros((1, nx), dtype=np.complex64)
    ref[0, 0] = 1.0
    x = np.zeros((1, nx), dtype=np.complex64)
    x[0, 3], x[0, 4], x[0, 11] = 1.0, 0.7, -0.4
    fine = np.asarray(
        Corr2D(ref=ref, dwell=1, nx_out=nx * up).execute(x.ravel())
    )
    d.real_stays_real = float(np.max(np.abs(fine.imag)))
    d.real_peak = float(np.max(np.abs(fine.real)))
    R.md(
        f"Worst imaginary part over the interpolated grid: "
        f"**{d.real_stays_real:.2e}**, against a real peak of "
        f"{d.real_peak:.3f}. With the split removed the same measurement "
        f"reads 6.3e-03 — five orders of magnitude, which is what makes "
        f"this a gate rather than a tolerance."
    )
    R.md()


def _sec_accuracy(d: Data) -> None:
    R.md("### 2.6 Accuracy against size")
    R.md()
    R.md(
        "float32 transform error grows with the transform length. This is "
        "the envelope a caller inherits when they size a code period."
    )
    R.md()
    rows, csv = [], []
    worst = 0.0
    for tag, (ny, nx) in enumerate([(1, 16), (1, 64), (4, 32), (8, 64)]):
        r = _rng(300 + tag)
        x = _cplx(r, ny, nx)
        ref = np.zeros((ny, nx), dtype=np.complex64)
        ref[0] = _cplx(r, nx)
        got = np.asarray(Corr2D(ref=ref, dwell=1).execute(x.ravel())).reshape(
            ny, nx
        )
        want = _brute_corr2d(x, ref)
        err = float(np.max(np.abs(got - want)) / np.max(np.abs(want)))
        worst = max(worst, err)
        rows.append([f"{ny}x{nx}", str(ny * nx), f"{err:.2e}"])
        csv.append([ny, nx, ny * nx, err])
    R.table(["shape", "points", "max relative error"], rows)
    _csv(HERE / "data" / "accuracy.csv", "ny,nx,points,rel_err", csv)
    d.accuracy_rows = rows
    d.accuracy_worst = worst
    R.md(
        f"Worst **{worst:.2e}** at the sizes swept. Raw sweep: "
        f"`data/accuracy.csv`."
    )
    R.md()


def _sec_lifecycle(d: Data) -> None:
    R.md("### 2.7 reset, and the state triplet")
    R.md()
    R.md(
        "`reset` is measured as the header states it — *equivalent to "
        "starting a fresh dwell cycle* — rather than by reading the counter "
        "back. A partial dwell goes into one object, it is reset, and then "
        "it and an untouched object run the same full cycle: the dumps must "
        "agree. Reading `count` on a freshly created object, which is what "
        "the C test did, passes against a `reset` with an empty body."
    )
    R.md()
    ny, nx = 4, 4
    r = _rng(400)
    ref = np.zeros((ny, nx), dtype=np.complex64)
    ref[0] = _cplx(r, nx)
    stale, fresh = _cplx(r, ny, nx), _cplx(r, ny, nx)
    a, b = Corr2D(ref=ref, dwell=2), Corr2D(ref=ref, dwell=2)
    a.execute(stale.ravel())
    a.reset()
    a.execute(fresh.ravel())
    b.execute(fresh.ravel())
    da = np.asarray(a.execute(fresh.ravel()))
    db = np.asarray(b.execute(fresh.ravel()))
    d.reset_clears = bool(np.array_equal(da, db))
    R.md(
        f"Bit-identical after reset: **{d.reset_clears}** — a stale "
        f"accumulator would carry the discarded frame into the dump."
    )
    R.md()
    c = Corr2D(ref=ref, dwell=3)
    c.execute(fresh.ravel())
    blob = c.get_state()
    c2 = Corr2D(ref=ref, dwell=3)
    c2.set_state(blob)
    c.execute(stale.ravel())
    c2.execute(stale.ravel())
    o1 = np.asarray(c.execute(stale.ravel()))
    o2 = np.asarray(c2.execute(stale.ravel()))
    d.state_exact = bool(np.array_equal(o1, o2))
    bad = bytearray(blob)
    bad[0] ^= 0xFF
    try:
        c2.set_state(bytes(bad))
        d.state_rejects = False
    except ValueError:
        d.state_rejects = True
    R.md(
        f"A mid-dwell blob resumes bit-exactly into a fresh instance "
        f"(**{d.state_exact}**), and a clobbered envelope is rejected "
        f"(**{d.state_rejects}**) rather than reinterpreted."
    )
    R.md()


def _sec_bounds(d: Data) -> None:
    R.md("### 2.8 Output sizing")
    R.md()
    ny, nx = 4, 4
    r = _rng(500)
    ref = np.zeros((ny, nx), dtype=np.complex64)
    ref[0] = _cplx(r, nx)
    c = Corr2D(ref=ref, dwell=1)
    d.n_out_matches = c.execute_max_out() == ny * nx and c.n_out == ny * nx
    fine = Corr2D(ref=ref, dwell=1, ny_out=8, nx_out=8)
    d.trunc_ok = fine.execute_max_out() == 64
    R.table(
        ["configuration", "execute_max_out()", "n_out"],
        [
            ["native 4x4", str(c.execute_max_out()), str(c.n_out)],
            ["interpolated 8x8", str(fine.execute_max_out()), str(fine.n_out)],
        ],
    )
    R.md(
        "`execute_max_out()` follows the OUTPUT grid, not the input frame — "
        "the number a caller sizes a buffer with when the inverse is larger "
        "than the forward."
    )
    R.md()

    R.md("### 2.9 Identities and refusals")
    R.md()
    # The correlation of a unit impulse with itself is a unit impulse --
    # the one case with a closed form needing no reference at all.
    imp = np.zeros((4, 4), dtype=np.complex64)
    imp[0, 0] = 1.0
    out = np.asarray(Corr2D(ref=imp, dwell=1).execute(imp.ravel()))
    want = np.zeros(16, dtype=np.complex64)
    want[0] = 1.0
    d.identity_err = float(np.max(np.abs(out - want)))

    # "Same peak": at an INTEGER offset the interpolated grid must agree
    # with the native one on the peak magnitude. (At a fractional offset it
    # legitimately exceeds it -- the native grid is sampling the shoulder.)
    nx = 16
    ref1 = np.zeros((1, nx), dtype=np.complex64)
    ref1[0, 0] = 1.0
    xi = _band_limited_impulse(nx, 3.0).reshape(1, nx)
    nat = np.asarray(Corr2D(ref=ref1, dwell=1).execute(xi.ravel()))
    fin = np.asarray(
        Corr2D(ref=ref1, dwell=1, nx_out=nx * 8).execute(xi.ravel())
    )
    d.peak_preserved = float(
        abs(np.max(np.abs(fin)) - np.max(np.abs(nat))) / np.max(np.abs(nat))
    )

    # reset() clears the run, never the configuration.
    c2 = Corr2D(ref=ref1, dwell=3, nx_out=nx * 2)
    before = (c2.dwell, c2.ny, c2.nx, c2.nx_out, c2.n_out)
    c2.execute(xi.ravel())
    c2.reset()
    d.reset_keeps_config = (
        c2.dwell,
        c2.ny,
        c2.nx,
        c2.nx_out,
        c2.n_out,
    ) == before and c2.count == 0

    # An output grid SMALLER than the input is refused rather than
    # silently truncating a correlation surface.
    bad = 0
    for kwargs in ({"ny_out": 0, "nx_out": 8}, {"nx_out": 4}, {"dwell": 0}):
        try:
            Corr2D(ref=ref1, **kwargs)
        except (ValueError, MemoryError, TypeError):
            bad += 1
    d.rejects_bad_args = bad == 3

    R.table(
        ["claim", "measured"],
        [
            ["delta correlated with delta is delta", f"{d.identity_err:.2e}"],
            [
                "interpolated peak magnitude == native, at an integer offset",
                f"{d.peak_preserved:.2e}",
            ],
            [
                "reset keeps dwell/ny/nx/nx_out/n_out, clears count",
                str(d.reset_keeps_config),
            ],
            [
                "nx_out < nx, dwell=0 are refused (3 of 3)",
                str(d.rejects_bad_args),
            ],
        ],
    )
    R.md(
        "The peak-magnitude row is stated at an INTEGER offset on purpose: "
        "at a fractional one the interpolated peak legitimately EXCEEDS the "
        "native maximum, because the native grid was sampling the shoulder "
        "of a peak it could not represent. That is the feature, not a "
        'violation of "same peak".'
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "`corr2d_reset` was pinned **vacuously**, which is the shape "
        "`docs/dev/contributing/validation.md` warns about by name. The C "
        "test called it on a freshly created object and asserted "
        "`count == 0` — already true before the call — so a `reset()` with "
        "an empty body passed, and the header's other promise, that it "
        "zeroes the accumulator, was asserted by nothing. Now measured as "
        "the header states it (§2.7). Sabotage: dropping the `memset` takes "
        "the new check red and leaves the OLD one green, which is the "
        "vacuity demonstrated rather than argued.",
    )
    R.find(
        "F2",
        "FIXED",
        "The even-`n` Nyquist-bin split in `corr2d_zeropad_1d` — whose "
        "comment claims it matches `scipy.signal.resample` to machine "
        "precision — was covered by **nothing**. Proven by deleting it and "
        "watching the whole suite stay green, including the new sub-bin "
        "interpolation section, because the split changes the interpolated "
        "shape and barely moves its argmax. The invariant that sees it is "
        "realness (§2.5): 5.5e-08 with the split against 6.3e-03 without.",
    )
    R.find(
        "F3",
        "FIXED",
        "Sub-bin interpolation was pinned only at INTEGER shifts, where the "
        "native grid already lands on the answer. Now swept at quarter-bin "
        "offsets on both parities of `nx` (§2.4). Worth recording how the "
        "first attempt failed, because it reads exactly like an "
        "implementation defect: building the band-limited impulse with the "
        "phase ramp over `u = 0..nx-1` puts two equal peaks either side of "
        "a null at the true position. That sweep treats the upper bins as "
        "high POSITIVE frequencies rather than the negative ones they are, "
        "and no correlation surface has such a spectrum. With the "
        "frequencies signed the peak lands within a hundredth of a bin "
        "everywhere — the correlator was right and the stimulus was wrong.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        '`nthreads` is documented *"accepted for API compatibility; '
        'ignored"* and cannot be sabotaged: `corr2d_state_t` has no such '
        "member, `create()` forwards the argument to "
        "`fft_create`/`fft2d_create`, and both open with `(void)nthreads;`. "
        "So nothing stores it and there is no state to corrupt. The C test "
        "checks bit-identical output across four values anyway, described "
        "there as a forward guard rather than a proof — it fires the day "
        "someone wires it to a threaded reduction whose summation order "
        "differs, which is exactly when the documented no-op stops being "
        "one.",
    )
    R.find(
        "F6",
        "FIXED",
        '`corr2d_create` documented `dwell` as *"must be >= 1"* and '
        "enforced only the output-grid rule, so `dwell = 0` built an "
        "object that was not merely degenerate but **silent**: the dump "
        "test is `++count == dwell`, which zero never satisfies, so it "
        "accumulated every frame it was handed and emitted nothing until "
        "`count` wrapped at SIZE_MAX. A caller whose dwell came from a "
        "computed value that underflowed got silence and unbounded growth "
        "rather than a refusal. Found by §2.9's refusal check, which "
        "failed on the first run. Fixed at the primitive rather than in "
        "each caller: `detector2d_create` forwards its own `dwell` "
        "straight into this call, so both objects now refuse it and a "
        "second copy of the rule cannot drift. NULL is the whole error "
        "report by design — `docs/dev/contributing/error-convention.md` "
        "makes NULL the return for an invalid argument as well as an "
        "allocation failure, which the binding renders as `MemoryError`. "
        "Sabotage-proven: removing the check takes the new C section red.",
    )
    R.find(
        "F5",
        "C-ONLY",
        "Three claims have no Python face and are certified in "
        "`native/tests/test_corr2d_core.c` instead: `corr2d_set_ref` (no "
        "binding at all — including its contract that a fast-path object "
        "REJECTS a reference that is no longer single-row), the `fast_path` "
        "selection flag, and the native path's promise to allocate no "
        "interpolation scratch, which is only observable as NULL pointers. "
        "The last is now asserted with the padded path checked to allocate "
        "them, so it cannot pass on fields that are always NULL.",
    )


# ── 4. limits ─────────────────────────────────────────────────────────


def limits(d: Data) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not a "
        "new finding. Every one is asserted by "
        "`src/doppler/spectral/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.brute_worst < 1e-5,
        f"the correlation matches a brute-force circular correlation over 8 "
        f"cells and both reference shapes (worst {d.brute_worst:.2e})",
    )
    R.limit(
        d.dump_schedule_ok,
        "a dump lands on the dwell-th call and on no earlier one, at "
        "dwell 1, 2, 3 and 5",
    )
    R.limit(
        d.dwell_is_sum < 1e-6,
        f"the dump is the coherent SUM of the dwell — dwell=3 is exactly 3x "
        f"dwell=1 on a repeated frame ({d.dwell_is_sum:.2e})",
    )
    R.limit(
        d.fast_matches_general < 1e-5,
        f"the single-row fast path matches the brute-force truth, not "
        f"merely the general path (worst {d.fast_matches_general:.2e})",
    )
    R.limit(
        d.interp_worst_bins < 0.02,
        f"a larger inverse resolves a fractional peak to "
        f"{d.interp_worst_bins:.3f} of a bin, on both parities of nx",
    )
    R.limit(
        d.native_grid_worst >= 0.5 - 1e-9,
        f"the native grid genuinely cannot do that — off by "
        f"{d.native_grid_worst:.2f} of a bin at a half-bin offset",
    )
    R.limit(
        d.real_stays_real < 1e-5,
        f"the interpolation of a real correlation stays real "
        f"({d.real_stays_real:.2e}), which is what pins the Nyquist split",
    )
    R.limit(
        d.real_peak > 0.5,
        f"...and not because the output is zero (real peak {d.real_peak:.3f})",
    )
    R.limit(
        d.accuracy_worst < 1e-5,
        f"float32 accuracy holds to {d.accuracy_worst:.2e} up to 512 points",
    )
    R.limit(
        d.reset_clears,
        "reset() really starts a fresh dwell: a reset object and an "
        "untouched one produce bit-identical dumps",
    )
    R.limit(d.state_exact, "a mid-dwell state blob resumes bit-exactly")
    R.limit(
        d.state_rejects,
        "a clobbered state envelope is rejected, never reinterpreted",
    )
    R.limit(
        d.n_out_matches,
        "execute_max_out() and n_out are ny*nx on the native grid",
    )
    R.limit(
        d.trunc_ok,
        "execute_max_out() follows the OUTPUT grid when the inverse is "
        "larger than the forward",
    )
    R.limit(
        d.identity_err < 1e-6,
        f"a unit impulse correlated with itself is a unit impulse "
        f"({d.identity_err:.2e})",
    )
    R.limit(
        d.peak_preserved < 1e-3,
        f"at an integer offset the interpolated peak magnitude equals the "
        f"native one ({d.peak_preserved:.2e}) — the 'same peak' half of the "
        f"claim",
    )
    R.limit(
        d.reset_keeps_config,
        "reset() clears the run and keeps the configuration",
    )
    R.limit(
        d.rejects_bad_args,
        "an output grid smaller than the input, and dwell=0, are refused "
        "rather than silently accepted",
    )


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# Corr2D — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "Corr2D",
        [
            f"**The transform path is the brute-force answer** to "
            f"{d.brute_worst:.1e} across shapes and both reference kinds, "
            f"so the FFT machinery, the scaling and the conjugation "
            f"convention are all measured against something that shares "
            f"none of them (§2.1).",
            "**A dump is the coherent SUM, not an average.** `dwell=3` on a "
            "repeated frame is exactly three times `dwell=1`, and nothing "
            "is emitted before the dwell-th call — which is what a caller "
            "sizing an acquisition threshold has to assume (§2.2).",
            f"**A larger inverse buys real sub-bin resolution**: a "
            f"fractional peak is recovered to {d.interp_worst_bins:.3f} of "
            f"a bin where the native grid is off by "
            f"{d.native_grid_worst:.2f} (§2.4). Build the test stimulus "
            f"over SIGNED frequencies — a `0..nx-1` sweep produces a "
            f"double peak that reads like a broken interpolator and is not "
            f"(F3).",
            "**Three claims are C-ONLY**, including `set_ref`'s contract "
            "that a fast-path object refuses a reference that is no longer "
            "single-row. A Python-only audit of this object would report a "
            "clean bill of health for the surface that matters least (F5).",
            "**Two of the pins it arrived with could not fail**: `reset` "
            "was asserted on an object that was already reset, and the "
            "Nyquist split was covered by nothing at all. Both were found "
            "by breaking the code and watching the suite stay green, which "
            "is the only way that class of gap surfaces (F1, F2).",
        ],
    )
    R.summary(
        "\n- Raw sweeps: `data/brute_force.csv`, `data/interpolation.csv`, "
        "`data/accuracy.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
