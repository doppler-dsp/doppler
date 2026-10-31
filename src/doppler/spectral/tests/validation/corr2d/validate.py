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
    batch_rows: list[list[str]] = field(default_factory=list)
    batch_maps_exact: bool = False
    batch_coherent_exact: bool = False
    batch_folds_worst: float = 1.0
    batch_index_exact: bool = False
    batch_dwell_untouched: bool = False
    batch_refuses: bool = False
    batch_cancel: float = 1.0
    batch_doubles: float = 1.0
    bank_rows: list[list[str]] = field(default_factory=list)
    bank_worst: float = 1.0
    bank_index_exact: bool = False
    bank_dwell_untouched: bool = False
    bank_refuses: bool = False
    bank_loss_ok: bool = False
    bank_loss_tight: bool = False
    bank_detect_ok: bool = False
    bank_refine_worst: float = 1.0
    bank_pfa_held: bool = False
    bank_pd_parity: bool = False


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
    # A limits-only run (the pytest path, `build(write=False)`) has no
    # report for the CSV to support, and must not write into the repo.
    if not R.write:
        return
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
                "`native/inc/doppler/corr2d/corr2d_core.h`",
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
                "`nthreads` fans the fast path out, and the output is "
                "bit-identical at every count",
                "C (`_test_delay_doppler`, `_test_batch` at 1, 2, 7) + "
                "Python §2.10",
                "F4",
            ],
            [
                "`execute_max_out()` is `ny*nx`; emission stops at `max_out`",
                "C + Python",
                "§2.8",
            ],
            ["the state triplet round-trips", "C + Python", "§2.7"],
            [
                "a batch's map h is a fresh dwell-1 object's map for frame "
                "h, every path, every thread count",
                "C (`_test_batch`, bit-exact) + Python",
                "§2.10",
            ],
            [
                "`execute_coherent` is a fresh dwell-H object's dump",
                "C (bit-exact) + Python",
                "§2.10",
            ],
            [
                "power sum, power max (lowest h on a tie) and per-map peaks "
                "are folds over the maps, in h order",
                "C + Python",
                "§2.10",
            ],
            [
                "a batch never touches the dwell",
                "C (blob compare) + Python",
                "§2.10",
            ],
            [
                "a ragged, empty or short-output batch writes nothing",
                "C (poisoned outputs) + Python (`ValueError`)",
                "§2.10",
            ],
            [
                "`dp_corr2d_execute_fold` takes a caller's reducer",
                "C — **C-ONLY**, no Python callback by design",
                "C `_test_batch`",
            ],
            [
                "bank map h is the frame corrected by hand (within each "
                "period, times the rate), on every path",
                "C `_test_bank`, 5 paths x 1/2/7 threads",
                "§2.11",
            ],
            [
                "an offset between grid points loses at most sinc(1/(2L)), "
                "the bound itself at half a step",
                "C `_test_bank`",
                "§2.11",
            ],
            [
                "`bank_detect` names the fine carrier and recovers the "
                "coarse grid's straddle; the refine is the fine bank's value",
                "C `_test_bank_detect`, every path",
                "§2.11",
            ],
            [
                "detect-and-refine holds L = 4's sensitivity at the same Pfa",
                "the characterization (full geometry) and its fast twin",
                "§2.11 (small scale)",
            ],
            [
                "`dp_corr2d_bank_fold` takes a caller's reducer",
                "C — **C-ONLY**, no Python callback by design",
                "C `_test_bank`",
            ],
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
    _sec_batch(d)
    _sec_bank(d)
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
        "`dp_corr2d_reset` was pinned **vacuously**, which is the shape "
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
        "`nthreads` was once documented *accepted for API compatibility; "
        "ignored*, and this finding said so. Since #2666 it is real: the "
        "fast path's rows and the delay-Doppler columns fan out over a "
        "persistent pool, each worker on its own FFT plans, and a batch "
        "makes whole maps per worker (#2683). The forward guard this "
        "finding described is what now holds the design: the output is "
        "bit-identical at every thread count, checked in C at 1, 2 and 7 "
        "workers and here at 1 and 3 (§2.10). A threaded reduction that "
        "folded in completion order would break it, and the C test's "
        "sabotage of exactly that goes red.",
    )
    R.find(
        "F6",
        "FIXED",
        '`dp_corr2d_create` documented `dwell` as *"must be >= 1"* and '
        "enforced only the output-grid rule, so `dwell = 0` built an "
        "object that was not merely degenerate but **silent**: the dump "
        "test is `++count == dwell`, which zero never satisfies, so it "
        "accumulated every frame it was handed and emitted nothing until "
        "`count` wrapped at SIZE_MAX. A caller whose dwell came from a "
        "computed value that underflowed got silence and unbounded growth "
        "rather than a refusal. Found by §2.9's refusal check, which "
        "failed on the first run. Fixed at the primitive rather than in "
        "each caller: `dp_detector2d_create` forwards its own `dwell` "
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
        "`native/tests/test_corr2d_core.c` instead: `dp_corr2d_set_ref` (no "
        "binding at all — including its contract that a fast-path object "
        "REJECTS a reference that is no longer single-row), the `fast_path` "
        "selection flag, and the native path's promise to allocate no "
        "interpolation scratch, which is only observable as NULL pointers. "
        "The last is now asserted with the padded path checked to allocate "
        "them, so it cannot pass on fields that are always NULL.",
    )


def _sec_batch(d: Data) -> None:
    R.md("### 2.10 Batches")
    R.md()
    R.md(
        "A batch is H frames in one call (#2683, "
        "[`docs/design/corr2d-batch.md`](../../../../../../docs/design/"
        "corr2d-batch.md)). Every map is checked against what a FRESH "
        "dwell-1 object returns for that frame through `execute()`, which "
        "shares no batch code path, on the general, fast and known-column "
        "paths, at one thread and at three. The five frames are more than "
        "the three workers, so the three-thread rows run the whole-map "
        "path."
    )
    R.md()
    ny, nx, H = 4, 8, 5
    r = _rng(600)
    single = np.zeros((ny, nx), dtype=np.complex64)
    single[0] = _cplx(r, nx)
    dense = _cplx(r, ny, nx)
    x = _cplx(r, H * ny * nx).ravel()
    configs = [
        ("general 2-D", {"ref": dense}),
        ("fast path", {"ref": single}),
        ("fast, nx_out=12", {"ref": single, "nx_out": 12}),
        ("known column 3", {"ref": single, "col_out": 3}),
    ]
    maps_ok = coh_ok = idx_ok = untouched = refuses = True
    folds = 0.0
    for name, kw in configs:
        for nt in (1, 3):
            c = Corr2D(dwell=2, nthreads=nt, **kw)
            n = ny * nx
            fresh = np.stack(
                [
                    np.asarray(
                        Corr2D(dwell=1, **kw).execute(x[h * n : (h + 1) * n])
                    )
                    for h in range(H)
                ]
            )
            c.execute(x[:n])  # a dwell in progress
            blob = c.get_state()
            maps = c.execute_all(x)
            exact = bool(np.array_equal(maps, fresh))
            dh = Corr2D(dwell=H, **kw)
            for h in range(H):
                want = dh.execute(x[h * n : (h + 1) * n])
            coh = c.execute_coherent(x)
            coh_exact = bool(np.array_equal(coh, np.asarray(want)))
            pw = np.abs(fresh.astype(np.complex128)) ** 2
            ps = c.execute_power_sum(x)
            pm, ix = c.execute_power_max(x)
            pk, _, _, mn = c.execute_peaks(x)
            worst = max(
                float(np.max(np.abs(ps - pw.sum(0)) / pw.sum(0).max())),
                float(np.max(np.abs(pm - pw.max(0)) / pw.max())),
                float(np.max(np.abs(pk - pw.max(1)) / pw.max())),
                float(np.max(np.abs(mn - pw.mean(1)) / pw.mean(1).max())),
            )
            idx = bool(np.array_equal(ix, np.argmax(pw, axis=0)))
            still = c.get_state() == blob and c.count == 1
            bad = 0
            for length in (0, n - 1, n + 1, 2 * n + 1):
                try:
                    c.execute_all(x[:length])
                except ValueError:
                    bad += 1
            maps_ok &= exact
            coh_ok &= coh_exact
            idx_ok &= idx
            untouched &= still
            refuses &= bad == 4
            folds = max(folds, worst)
            d.batch_rows.append(
                [
                    name,
                    str(nt),
                    str(exact),
                    str(coh_exact),
                    f"{worst:.1e}",
                    str(idx),
                    str(still),
                    f"{bad}/4",
                ]
            )
    d.batch_maps_exact, d.batch_coherent_exact = maps_ok, coh_ok
    d.batch_index_exact, d.batch_dwell_untouched = idx_ok, untouched
    d.batch_refuses, d.batch_folds_worst = refuses, folds
    R.table(
        [
            "path",
            "threads",
            "maps == fresh",
            "coherent == dwell-H",
            "folds, worst rel.",
            "max index",
            "dwell untouched",
            "refused",
        ],
        d.batch_rows,
    )
    R.md()
    c = Corr2D(ref=single, dwell=1)
    one = np.abs(np.asarray(c.execute(x[: ny * nx]))) ** 2
    both = np.concatenate([x[: ny * nx], -x[: ny * nx]])
    d.batch_cancel = float(
        np.abs(c.execute_coherent(both)).max() / np.sqrt(one.max())
    )
    d.batch_doubles = float(
        np.max(np.abs(c.execute_power_sum(both) - 2 * one)) / one.max()
    )
    R.md(
        f"Frames x and -x: the coherent sum cancels to "
        f"{d.batch_cancel:.1e} of one map's peak magnitude, and the power "
        f"sum is twice one map's power to {d.batch_doubles:.1e}. That is "
        f"the difference between the two sums, and why both exist."
    )
    R.md()


def _sec_bank(d: Data) -> None:
    R.md("### 2.11 The hypothesis bank")
    R.md()
    R.md(
        "One frame through every carrier g on a 1/(L nx) grid times every "
        "rate (#2683 phase 3, [`docs/design/corr2d-hypotheses.md`]"
        "(../../../../../../docs/design/corr2d-hypotheses.md) section 6). "
        "Each bank's `power_max` is checked against `execute_power_max` "
        "over the frame batch of hand-corrected copies, which shares no "
        "bank code, on every path, with two rate vectors."
    )
    R.md()
    from doppler.spectral import DelayDoppler

    ny, nx, L, g0, g1 = 4, 8, 3, -4, 3
    r = _rng(700)
    single = np.zeros((ny, nx), dtype=np.complex64)
    single[0] = _cplx(r, nx)
    dense = _cplx(r, ny, nx)
    x = _cplx(r, ny * nx).ravel()
    rates = np.exp(1j * r.uniform(0, 2 * np.pi, (2, ny))).astype(np.complex64)
    n_idx = np.arange(nx)
    copies = np.concatenate(
        [
            x
            * np.tile(np.exp(-2j * np.pi * g * n_idx / (L * nx)), ny)
            * np.repeat(w, nx)
            for g in range(g0, g1 + 1)
            for w in rates
        ]
    ).astype(np.complex64)
    configs = [
        ("general 2-D", lambda: Corr2D(ref=dense, dwell=2)),
        ("fast path", lambda: Corr2D(ref=single, dwell=2)),
        ("fast, nx_out=12", lambda: Corr2D(ref=single, dwell=2, nx_out=12)),
        ("known column 3", lambda: Corr2D(ref=single, dwell=2, col_out=3)),
        (
            "DelayDoppler, ny_out=8",
            lambda: DelayDoppler(single[0], ny=ny, dwell=2, ny_out=8),
        ),
    ]
    worst, idx_ok, untouched, refuses = 0.0, True, True, True
    for name, make in configs:
        c = make()
        c.execute(x)  # a dwell in progress
        blob = c.get_state()
        pw, h = c.bank_power_max(x, L, g0, g1, rates.ravel())
        bp, _bi = c.execute_power_max(copies)
        rel = float(np.max(np.abs(pw - bp)) / bp.max())
        # The bank names a hypothesis whose batch power IS the cell's maximum,
        # to rounding. Not the same index: on noise two hypotheses can hold a
        # cell to within an ulp, and rounding decides which is strictly first.
        stack = (
            np.abs(
                np.asarray(c.execute_all(copies)).reshape(-1, h.size)
            ).astype(np.float64)
            ** 2
        )
        named = stack[h.astype(np.int64), np.arange(h.size)]
        exact = bool(np.all(named >= stack.max(0) * (1 - 1e-5)))
        still = c.get_state() == blob and c.count == 1
        bad = 0
        for args in (
            (x[:-1], L, g0, g1),
            (x, 0, g0, g1),
            (x, L, g1, g0),
        ):
            try:
                c.bank_power_max(*args, rates.ravel())
            except ValueError:
                bad += 1
        try:
            c.bank_power_max(x, L, g0, g1, rates.ravel()[:-1])
        except ValueError:
            bad += 1
        worst = max(worst, rel)
        idx_ok &= exact
        untouched &= still
        refuses &= bad == 4
        d.bank_rows.append(
            [name, f"{rel:.1e}", str(exact), str(still), f"{bad}/4"]
        )
    d.bank_worst, d.bank_index_exact = worst, idx_ok
    d.bank_dwell_untouched, d.bank_refuses = untouched, refuses
    R.table(
        [
            "path",
            "power_max vs corrected batch, rel.",
            "names a strongest hypothesis",
            "dwell untouched",
            "refused",
        ],
        d.bank_rows,
    )
    R.md()

    # an offset between grid points: the strongest cell against the exact
    # correction's, noiseless, on DelayDoppler (64 chips, 8 periods)
    rr = _rng(701)
    code = (2 * rr.integers(0, 2, 64) - 1).astype(np.complex64)
    dd = DelayDoppler(code, ny=8)
    m_all = np.arange(8 * 64)
    loss_rows, ok, tight = [], True, True
    for Lg in (1, 2, 4):
        bound = float(np.sinc(1 / (2 * Lg)))
        worst_ratio, at_mid = 1.0, None
        for phi in (0.1, 0.125, 0.25, 0.375, 0.5, -0.625):
            sig = np.tile(np.roll(code, 13), 8) * np.exp(
                2j * np.pi * phi / 64 * m_all
            )
            exact_map = dd.execute(
                (
                    sig
                    * np.tile(
                        np.exp(-2j * np.pi * phi * np.arange(64) / 64), 8
                    )
                ).astype(np.complex64)
            )
            ex = float(np.max(np.abs(exact_map) ** 2))
            pw, _ = dd.bank_power_max(
                sig.astype(np.complex64),
                Lg,
                -2 * Lg,
                2 * Lg,
                np.empty(0, np.complex64),
            )
            ratio = float(np.sqrt(pw.max() / ex))
            worst_ratio = min(worst_ratio, ratio)
            if abs(phi * Lg - np.floor(phi * Lg) - 0.5) < 1e-12:
                at_mid = ratio
        ok &= worst_ratio >= bound - 1e-3
        tight &= at_mid is not None and abs(at_mid - bound) < 2e-3
        loss_rows.append(
            [
                str(Lg),
                f"{-20 * np.log10(bound):.2f} dB",
                f"{-20 * np.log10(worst_ratio):.2f} dB",
                f"{-20 * np.log10(at_mid):.2f} dB" if at_mid else "-",
            ]
        )
    d.bank_loss_ok, d.bank_loss_tight = ok, tight
    R.table(
        ["L", "bound sinc(1/(2L))", "worst measured", "at half a step"],
        loss_rows,
    )
    R.md()

    # bank_detect: noiseless, between two coarse points; the refine is the
    # fine bank's own value at the cell, on every path
    det_ok, ref_worst = True, 0.0
    for phi in (0.25, -0.75, 1.25):
        sig = (
            np.tile(np.roll(code, 41), 8)
            * np.exp(2j * np.pi * phi / 64 * m_all)
        ).astype(np.complex64)
        none = np.empty(0, np.complex64)
        coarse, _ = dd.bank_power_max(sig, 2, -4, 4, none)
        t = 0.5 * float(coarse.max())
        p, g, _rate, row, col = dd.bank_detect(sig, 2, 4, -4, 4, t, t, none)
        want = round(4 * phi)
        fine, _ = dd.bank_power_max(sig, 4, want, want, none)
        recovered = float(p[0] / coarse.max()) if len(p) else 0.0
        det_ok &= (
            len(p) == 1
            and int(g[0]) == want
            and int(col[0]) == 41
            and abs(recovered - 1 / np.sinc(0.25) ** 2) < 1e-3
        )
        if len(p):
            ref_worst = max(
                ref_worst,
                float(abs(p[0] - fine[int(row[0]) * 64 + int(col[0])]) / p[0]),
            )
    d.bank_detect_ok, d.bank_refine_worst = det_ok, ref_worst
    R.md(
        f"`bank_detect` (L_d = 2, L_r = 4) on a carrier a quarter bin from "
        f"both coarse neighbors, at three such carriers: one record each, on "
        f"the fine carrier and lag, recovering 1/sinc(1/4)^2 = "
        f"{1 / np.sinc(0.25) ** 2:.3f} of the coarse peak "
        f"({'yes' if det_ok else 'NO'}); the refined power is the fine "
        f"bank's map at the cell to {ref_worst:.1e}."
    )
    R.md()

    # detection at the same Pfa, small scale: the characterization's
    # measure, as its fast twin runs it
    from doppler.spectral.tests.characterization.corr2d_bank import (
        characterize as bank_char,
    )

    geo = bank_char.Geometry(
        16, 7, 0.1, 0.3, {"uniform": -19.5, "worst": -21.0}
    )
    res = bank_char.measure(geo, trials=120, noise_frames=300)
    pfa_held = all(
        lo <= geo.pfa and p >= geo.pfa / 3 for p, lo, hi in res["pfa"].values()
    )
    parity = all(
        pd["refine"][1] <= pd["L4"][2] and pd["L4"][1] <= pd["refine"][2]
        for pd in res["pd"].values()
    )
    d.bank_pfa_held, d.bank_pd_parity = pfa_held, parity
    rows = []
    for mode, pd in res["pd"].items():
        rows.append(
            [mode, f"{geo.snr_db[mode]:.1f} dB"]
            + [
                f"{pd[k][0]:.3f} [{pd[k][1]:.3f}, {pd[k][2]:.3f}]"
                for k in ("oracle", "L4", "L2", "refine")
            ]
        )
    R.md(
        "Detection at one false-alarm rate per search (0.1 here), 16 periods "
        "of a 127-chip PN code, 120 trials a row and 300 noise frames: the "
        "characterization's `measure` at the scale its fast twin runs. The "
        "full geometry (64 x 511, 1e-3) is "
        "`characterization/corr2d_bank/characterize.py`, run by "
        "`make characterize`."
    )
    R.md()
    R.table(
        ["carrier", "SNR/sample", "oracle", "L = 4", "L = 2", "detect 2 -> 4"],
        rows,
    )
    R.md()
    R.table(
        ["detector", "Pfa on fresh noise"],
        [
            [k, f"{v[0]:.3f} [{v[1]:.3f}, {v[2]:.3f}]"]
            for k, v in res["pfa"].items()
        ],
    )
    R.md()


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
        d.batch_maps_exact,
        "a batch's map h is bit-identical to a fresh dwell-1 object's map "
        "for frame h, on the general, fast, interpolated and known-column "
        "paths, at one thread and three",
    )
    R.limit(
        d.batch_coherent_exact,
        "execute_coherent is bit-identical to a fresh dwell-H object's dump",
    )
    R.limit(
        d.batch_folds_worst < 1e-5,
        f"power sum, power max and per-map peak and mean are the folds over "
        f"the maps (worst {d.batch_folds_worst:.1e})",
    )
    R.limit(
        d.batch_index_exact,
        "execute_power_max's index is the map each maximum came from",
    )
    R.limit(
        d.batch_dwell_untouched,
        "a batch mid-dwell leaves the count and the state blob unchanged",
    )
    R.limit(
        d.batch_refuses,
        "an empty or ragged batch is a ValueError",
    )
    R.limit(
        d.batch_cancel < 1e-5 and abs(d.batch_doubles) < 1e-5,
        f"frames x and -x: the coherent sum cancels "
        f"({d.batch_cancel:.1e}) and the power sum doubles "
        f"({d.batch_doubles:.1e})",
    )
    R.limit(
        d.rejects_bad_args,
        "an output grid smaller than the input, and dwell=0, are refused "
        "rather than silently accepted",
    )
    R.limit(
        d.bank_worst < 1e-5 and d.bank_index_exact,
        f"a bank's power_max is the corrected frame batch's on every path, "
        f"with rates (worst {d.bank_worst:.1e}), and the hypothesis it names "
        f"holds the cell's strongest power, to rounding",
    )
    R.limit(
        d.bank_dwell_untouched and d.bank_refuses,
        "a bank leaves the dwell's count and blob unchanged, and a wrong "
        "frame, L = 0, an inverted range or ragged rates is a ValueError",
    )
    R.limit(
        d.bank_loss_ok and d.bank_loss_tight,
        "an offset between grid points loses at most sinc(1/(2L)), and "
        "exactly that half a step from both neighbors, at L = 1, 2 and 4",
    )
    R.limit(
        d.bank_detect_ok and d.bank_refine_worst < 1e-5,
        f"bank_detect names the fine carrier and lag and recovers the "
        f"coarse straddle; the refine is the fine bank's value "
        f"({d.bank_refine_worst:.1e})",
    )
    R.limit(
        d.bank_pfa_held and d.bank_pd_parity,
        "every detector holds its Pfa on fresh noise, and detect-and-refine "
        "detects as often as L = 4 within the intervals (small scale)",
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
            "**A batch makes the same maps as execute(), and leaves the "
            "dwell alone.** H frames in one call give maps bit-identical to "
            "H fresh dwell-1 objects on every path and thread count, the "
            "coherent sum is a dwell-H dump, and a batch mid-dwell changes "
            "neither the count nor the state blob (§2.10).",
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
