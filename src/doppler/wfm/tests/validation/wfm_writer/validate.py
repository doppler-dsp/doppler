"""Writer — certification evidence, measured through the binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/wfm/tests/validation/wfm_writer/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's: `native/inc/wfm_writer/wfm_writer_core.h`
is the SSOT and `native/tests/test_wfm_writer_core.c` certifies it in C.
This file measures the same properties through `doppler.wfm.Writer`.

**The truth here is the file on disk, read by numpy.** A capture writer's
whole job is that the bytes it produced mean what the header says, so
scoring it with `doppler.wfm.Reader` would ask a weaker question -- the
two share the `SCALE` table and a shared error would cancel, which is
exactly the trap `docs/dev/contributing/validation.md` names. So every
sample here is read back with `np.fromfile` at the dtype the wire type
declares, and compared against arithmetic. `Reader` appears once, in
§2.6, and only to establish that the two agree.
"""

from __future__ import annotations

import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import Reader, Writer

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

FS = 1.0e6

# The five complex wire types, with the numpy dtype each lands as and the
# full-scale constant the writer maps +-1.0 onto. The dtype is how the file
# is read back without asking doppler what it wrote.
WIRE = [
    ("cf32", "<c8", None),
    ("cf64", "<c16", None),
    ("ci32", "<i4", 2147483647.0),
    ("ci16", "<i2", 32767.0),
    ("ci8", "<i1", 127.0),
]
INTEGER = [(n, d, s) for n, d, s in WIRE if s is not None]

N = 4096


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def signal(n: int = N, seed: int = 7) -> np.ndarray:
    """A deterministic test signal that fills the range without clipping.

    Uniform rather than Gaussian on purpose: a quantiser's error statistic
    is a property of the grid, and a uniform input exercises every part of
    it equally instead of concentrating on the middle.
    """
    rng = np.random.default_rng(seed)
    return (rng.uniform(-0.9, 0.9, n) + 1j * rng.uniform(-0.9, 0.9, n)).astype(
        np.complex64
    )


def write_capture(tmp: Path, x: np.ndarray, stype: str, **kw) -> Path:
    """Write one raw capture and return its path."""
    p = tmp / f"cap_{stype}_{kw.get('headroom', 0.0)}.raw"
    w = Writer(p, file_type="raw", sample_type=stype, fs=FS, **kw)
    w.write(x)
    w.close()
    return p


def read_components(path: Path, dtype: str) -> np.ndarray:
    """The file, as a flat real component array, read by numpy alone."""
    a = np.fromfile(path, dtype=dtype)
    if np.iscomplexobj(a):
        out = np.empty(2 * a.size, np.float64)
        out[0::2] = a.real
        out[1::2] = a.imag
        return out
    return a.astype(np.float64)


def interleave(x: np.ndarray) -> np.ndarray:
    """Complex samples as the I,Q,I,Q component order the wire uses."""
    out = np.empty(2 * x.size, np.float64)
    out[0::2] = x.real
    out[1::2] = x.imag
    return out


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    wire_rows: list[list[str]] = field(default_factory=list)
    wire_all_exact: bool = False
    quant_rows: list[list[float]] = field(default_factory=list)
    quant_table: list[list[str]] = field(default_factory=list)
    all_truncate: bool = False
    any_round: bool = False
    worst_lsb: float = 0.0
    trunc_rms: float = 0.0
    round_rms: float = 0.0
    penalty_db: float = 0.0
    ci8_floor: float = 0.0
    ci8_floor_rounded: float = 0.0
    peak_rows: list[list[str]] = field(default_factory=list)
    peak_exact: bool = False
    clipped_rule: bool = False
    clip_rows: list[list[str]] = field(default_factory=list)
    clip_exact: bool = False
    fraction_off_is_zero: bool = False
    headroom_rows: list[list[str]] = field(default_factory=list)
    remedy_clears: bool = False
    remedy_bites: bool = False
    snr_rows: list[list[float]] = field(default_factory=list)
    snr_invariant_db: float = 0.0
    unity_identical: bool = False
    reader_agrees: bool = False
    reader_rows: list[list[str]] = field(default_factory=list)
    hint_rows: list[list[str]] = field(default_factory=list)
    hint_silent: int = 0
    hint_total: int = 0
    hint_all_wrong_type: bool = False
    fs_lost: float = -1.0
    unreachable: list[str] = field(default_factory=list)


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`Writer` is where a generated waveform stops being samples in "
        "memory and becomes a file another program will read -- raw "
        "interleaved I/Q, CSV, BLUE type-1000, or SigMF. The design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md); "
        "the API is `native/inc/wfm_writer/wfm_writer_core.h`, certified in "
        "C by `native/tests/test_wfm_writer_core.c`. Neither is restated "
        "here."
    )
    R.md()
    R.md(
        "Everything below is measured offline -- a temporary directory and "
        "numpy -- so this report renders identically on any machine, which "
        "is what `make validate-check` requires of it."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_wire(d: Data, tmp: Path) -> None:
    R.md("### 2.1 Five wire types, read back by numpy (C §raw/endian)")
    R.md()
    R.md(
        "The writer's contract is that `+-1.0` maps to the type's full "
        "scale. Read back with `np.fromfile` at the declared dtype -- not "
        "through `Reader`, which shares the writer's own scale table."
    )
    R.md()
    x = signal(256)
    v = interleave(x)
    ok_all = True
    for name, dtype, scale in WIRE:
        got = read_components(write_capture(tmp, x, name), dtype)
        if scale is None:
            want = v.astype(np.float32).astype(np.float64)
            exact = bool(np.allclose(got, want, atol=1e-6))
            note = "float, stored as-is"
        else:
            want = np.trunc(v * scale)
            exact = bool(np.array_equal(got, want))
            note = f"+-1.0 -> +-{scale:.0f}"
        ok_all = ok_all and exact
        d.wire_rows.append(
            [
                name,
                dtype,
                str(got.size),
                note,
                "exact" if exact else "**DIFFERS**",
            ]
        )
    d.wire_all_exact = ok_all
    R.table(
        ["type", "dtype", "components", "mapping", "vs arithmetic"],
        d.wire_rows,
    )
    R.md(
        "Every type lands exactly where the arithmetic says, and the "
        "component count is `2 * n` in each -- the file is interleaved "
        "I/Q with no padding and no header (`raw`)."
    )
    R.md()


def measure_quantisation(d: Data, tmp: Path) -> None:
    R.md("### 2.2 The quantiser truncates, and what that costs (F1)")
    R.md()
    R.md(
        "The interesting question about an integer capture is not whether "
        "it round-trips but how much of the wire's dynamic range it "
        "spends. doppler's canonical converter, `f32_to_i16`, says it "
        '"rounds to the nearest integer". The writer does not call it: '
        "`wfm_writer_core.c` has its own `(long)(v * scale)`, which "
        "**truncates toward zero**."
    )
    R.md()
    R.md(
        "Both models are evaluated in numpy and the file is compared "
        "against each, so this is a measurement rather than a reading of "
        "the source."
    )
    R.md()
    x = signal()
    v = interleave(x)
    all_trunc, any_round = True, False
    for name, dtype, scale in INTEGER:
        got = read_components(write_capture(tmp, x, name), dtype)
        trunc = np.trunc(v * scale)
        rnd = np.round(v * scale)
        is_t = bool(np.array_equal(got, trunc))
        is_r = bool(np.array_equal(got, rnd))
        all_trunc = all_trunc and is_t
        any_round = any_round or is_r
        err = got - v * scale
        rerr = rnd - v * scale
        t_rms = float(np.sqrt(np.mean(err**2)))
        r_rms = float(np.sqrt(np.mean(rerr**2)))
        floor = 20 * np.log10(t_rms / scale)
        rfloor = 20 * np.log10(r_rms / scale)
        d.quant_table.append(
            [
                name,
                "yes" if is_t else "no",
                "yes" if is_r else "no",
                f"{np.max(np.abs(err)):.3f}",
                f"{t_rms:.4f}",
                f"{floor:.1f}",
                f"{rfloor:.1f}",
            ]
        )
        d.quant_rows.append([scale, t_rms, r_rms, floor, rfloor])
        d.worst_lsb = max(d.worst_lsb, float(np.max(np.abs(err))))
        if name == "ci8":
            d.ci8_floor, d.ci8_floor_rounded = float(floor), float(rfloor)
        d.trunc_rms, d.round_rms = t_rms, r_rms
    d.all_truncate, d.any_round = all_trunc, any_round
    d.penalty_db = float(20 * np.log10(d.trunc_rms / d.round_rms))
    R.table(
        [
            "type",
            "matches truncation",
            "matches rounding",
            "max err (LSB)",
            "rms (LSB)",
            "noise floor dBFS",
            "if rounded",
        ],
        d.quant_table,
    )
    R.md(
        f"Truncation on every integer type, and the cost is the same on "
        f"each: **{d.penalty_db:.1f} dB**. That is not a coincidence of "
        "this signal -- truncation toward zero spreads the error over a "
        "whole LSB (rms `1/sqrt(3)` = 0.577) where round-to-nearest "
        "spreads it over half (`1/sqrt(12)` = 0.289), and the ratio is "
        f"exactly 2, which is 6.02 dB. Measured {d.trunc_rms:.4f} against "
        f"{d.round_rms:.4f}."
    )
    R.md()
    R.md(
        f"It bites hardest where there is least to spare: an 8-bit "
        f"capture sits at **{d.ci8_floor:.1f} dBFS** where it could sit at "
        f"{d.ci8_floor_rounded:.1f}. Recorded as F1 and filed as gh-1117, "
        "which also names the two sibling copies in `wfm_sink` and "
        "`wfm_reader`."
    )
    R.md()
    R.md("![What truncating costs, per wire type](quantisation.png)")
    R.md()
    _csv(
        DATA / "quantisation.csv",
        "full_scale,trunc_rms_lsb,round_rms_lsb,floor_dbfs,rounded_dbfs",
        d.quant_rows,
    )


def measure_peak(d: Data, tmp: Path) -> None:
    R.md("### 2.3 peak, clipped and the clip fraction (C §accessors)")
    R.md()
    R.md(
        "The three properties a caller reads to answer one question -- did "
        "this capture survive its own level? `peak_dbfs` is "
        "`20*log10(peak)` over the largest axis written, and it is "
        "reported for float captures too, which is where this object and "
        "`StreamSink` deliberately differ (F3)."
    )
    R.md()
    exact = True
    for amp in (0.25, 0.5, 0.9, 1.5):
        x = np.full(64, amp + 0.1j, np.complex64)
        p = tmp / f"pk_{amp}.raw"
        w = Writer(p, file_type="raw", sample_type="ci16", fs=FS)
        w.write(x)
        want = 20 * np.log10(amp)
        got = w.peak_dbfs
        ok = abs(got - want) < 1e-6
        exact = exact and ok
        d.peak_rows.append(
            [
                f"{amp}",
                f"{want:+.4f}",
                f"{got:+.4f}",
                "yes" if w.clipped else "no",
            ]
        )
        w.close()
    d.peak_exact = exact
    R.table(
        ["written amplitude", "20*log10 dBFS", "peak_dbfs", "clipped"],
        d.peak_rows,
    )
    R.md()
    R.md(
        "`clipped` is a rule about the WIRE TYPE, not only the level: the "
        "same 1.5 that saturates an integer capture is merely loud in a "
        "float one."
    )
    R.md()
    rows = []
    rule = True
    for name, _dtype, scale in WIRE:
        p = tmp / f"cl_{name}.raw"
        w = Writer(p, file_type="raw", sample_type=name, fs=FS)
        w.write(np.full(32, 1.5 + 0.5j, np.complex64))
        want = scale is not None
        rule = rule and (w.clipped == want)
        rows.append(
            [
                name,
                "integer" if want else "float",
                "yes" if w.clipped else "no",
            ]
        )
        w.close()
    d.clipped_rule = rule
    R.table(["type", "kind", "clipped at 1.5"], rows)
    R.md()
    R.md(
        "The clipped *fraction* is the one extra per-sample compare, so it "
        "is opt-in. Its denominator is COMPONENTS, two per sample, which "
        "makes a block whose I saturates and whose Q does not exactly one "
        "half -- a number that needs no reference implementation, only "
        "counting."
    )
    R.md()
    cases = [
        ("nothing saturates", 0.5 + 0.25j, 0.0),
        ("I saturates, Q does not", 1.5 + 0.5j, 0.5),
        ("both saturate", 1.5 - 2.0j, 1.0),
    ]
    ok = True
    for what, val, want in cases:
        p = tmp / f"cf_{want}.raw"
        w = Writer(p, file_type="raw", sample_type="ci16", fs=FS)
        w.track_clipping(1)
        w.write(np.full(40, val, np.complex64))
        got = w.clip_fraction
        ok = ok and abs(got - want) < 1e-12
        d.clip_rows.append([what, f"{want:.2f}", f"{got:.2f}"])
        w.close()
    d.clip_exact = ok
    R.table(["block", "expected", "measured"], d.clip_rows)
    # and off by default
    p = tmp / "cf_off.raw"
    w = Writer(p, file_type="raw", sample_type="ci16", fs=FS)
    w.write(np.full(40, 1.5 - 2.0j, np.complex64))
    d.fraction_off_is_zero = w.clip_fraction == 0.0 and w.clipped
    w.close()
    R.md(
        "Off by default the same fully-saturated block reports 0.0 while "
        "`clipped` still reads true -- the peak is free, the counter is "
        "not, and only the counter is opt-in."
    )
    R.md()


def measure_headroom(d: Data, tmp: Path) -> None:
    R.md("### 2.4 Headroom: the remedy is exact, and costs no SNR")
    R.md()
    R.md(
        "The header states the remedy for a clipped capture with a number "
        'in it -- "exactly `ceil(20*log10(peak))` dB of headroom" -- so '
        "it can be checked rather than believed. Apply it, and the capture "
        "must clear full scale; apply one dB less, and it must not."
    )
    R.md()
    clears, bites = True, True
    for amp in (1.05, 1.5, 2.0, 3.7):
        x = np.full(64, amp + 0.0j, np.complex64)
        remedy = float(np.ceil(20 * np.log10(amp)))
        p = tmp / f"hr_{amp}.raw"
        w = Writer(
            p, file_type="raw", sample_type="ci16", fs=FS, headroom=remedy
        )
        w.write(x)
        after = 10 ** (w.peak_dbfs / 20.0)
        clears = clears and after <= 1.0 + 1e-9 and not w.clipped
        w.close()
        short = "n/a"
        if remedy >= 2.0:
            p2 = tmp / f"hr_{amp}_short.raw"
            w2 = Writer(
                p2,
                file_type="raw",
                sample_type="ci16",
                fs=FS,
                headroom=remedy - 1.0,
            )
            w2.write(x)
            still = w2.clipped
            bites = bites and still
            short = "still clips" if still else "**cleared**"
            w2.close()
        d.headroom_rows.append(
            [
                f"{amp}",
                f"{remedy:.0f}",
                f"{after:.4f}",
                "cleared" if after <= 1.0 else "**still over**",
                short,
            ]
        )
    d.remedy_clears, d.remedy_bites = clears, bites
    R.table(
        ["peak", "remedy dB", "peak after", "verdict", "one dB less"],
        d.headroom_rows,
    )
    R.md()
    R.md(
        "And the header's other half: headroom \"does not change any power "
        'ratio (SNR is invariant); it only moves the absolute level". '
        "Measured on a tone in noise, comparing the in-band to "
        "out-of-band power ratio of the file at each backoff."
    )
    R.md()
    rng = np.random.default_rng(3)
    n = 8192
    t = np.arange(n)
    sig = 0.5 * np.exp(2j * np.pi * 0.1 * t)
    noise = 0.02 * (rng.standard_normal(n) + 1j * rng.standard_normal(n))
    x = (sig + noise).astype(np.complex64)
    snrs = []
    for h in (0.0, 3.0, 6.0, 12.0):
        p = tmp / f"snr_{h}.raw"
        w = Writer(p, file_type="raw", sample_type="cf32", fs=FS, headroom=h)
        w.write(x)
        w.close()
        y = np.fromfile(p, dtype="<c8").astype(np.complex128)
        sp = np.abs(np.fft.fft(y)) ** 2
        k = round(0.1 * n)
        band = sp[k - 2 : k + 3].sum()
        snr = 10 * np.log10(band / (sp.sum() - band))
        snrs.append(snr)
        d.snr_rows.append([h, float(snr)])
    d.snr_invariant_db = float(max(snrs) - min(snrs))
    R.table(
        ["headroom dB", "measured SNR dB"],
        [[f"{h:.0f}", f"{s:.4f}"] for h, s in d.snr_rows],
    )
    R.md(
        f"The SNR moves by {d.snr_invariant_db:.2e} dB across 12 dB of "
        "backoff -- a single scale multiplies signal and noise alike, so "
        "the ratio is untouched and only the level moves."
    )
    R.md()
    _csv(DATA / "headroom_snr.csv", "headroom_db,snr_db", d.snr_rows)
    R.md(
        "Headroom 0 is documented as a bit-exact no-op. Written twice per "
        "wire type -- once with the argument omitted, once with `0.0` -- "
        "and compared byte for byte."
    )
    R.md()
    same = True
    x = signal(512)
    for name, _dtype, _scale in WIRE:
        a = tmp / f"u_a_{name}.raw"
        b = tmp / f"u_b_{name}.raw"
        wa = Writer(a, file_type="raw", sample_type=name, fs=FS)
        wb = Writer(b, file_type="raw", sample_type=name, fs=FS, headroom=0.0)
        wa.write(x)
        wb.write(x)
        wa.close()
        wb.close()
        same = same and a.read_bytes() == b.read_bytes()
    d.unity_identical = same
    R.md(
        "Byte-identical on all five"
        + ("." if same else " **-- except it is not.**")
    )
    R.md()


def measure_reader(d: Data, tmp: Path) -> None:
    R.md("### 2.5 Reading it back, and the sidecar nobody reads (F5)")
    R.md()
    R.md(
        "Everything above scored the writer against numpy. This asks the "
        "separate question of whether the library recovers its own "
        "capture -- and for the self-describing types it does, exactly."
    )
    R.md()
    x = signal(512)
    ok = True
    for ft, stype, hint in [
        ("blue", "ci16", None),
        ("blue", "cf32", None),
        ("csv", "cf32", None),
        ("raw", "ci16", "ci16"),
    ]:
        suffix = {"raw": ".raw", "blue": ".blue", "csv": ".csv"}[ft]
        p = tmp / f"rd_{ft}_{stype}{suffix}"
        w = Writer(p, file_type=ft, sample_type=stype, fs=FS)
        w.write(x)
        w.close()
        rd = Reader(p) if hint is None else Reader(p, sample_type=hint)
        got = np.asarray(rd.read(len(x)))
        tol = 1e-4 if stype == "ci16" else 1e-6
        agree = bool(got.size == x.size and np.allclose(got, x, atol=tol))
        ok = ok and agree
        d.reader_rows.append(
            [
                ft,
                stype,
                "carried" if hint is None else f"hint `{hint}`",
                str(got.size),
                "agrees" if agree else "**DIFFERS**",
            ]
        )
    d.reader_agrees = ok
    R.table(
        [
            "file type",
            "sample type",
            "how the type is known",
            "samples",
            "vs written",
        ],
        d.reader_rows,
    )
    R.md(
        "BLUE and SigMF carry their own type, so they need nothing. Raw "
        "and CSV are headerless, and the last row had to be TOLD -- which "
        "is the whole of F5."
    )
    R.md()

    R.md("#### What happens when it is not told")
    R.md()
    R.md(
        "The writer records the type in a `<path>.sigmf-meta` sidecar it "
        "writes beside every raw capture. The reader does not consult it. "
        "Read back at the default `cf32` hint, with the sidecar sitting "
        "next to the file saying otherwise:"
    )
    R.md()
    rows = []
    silent = 0
    total = 0
    for n in (512, 4096, 513):
        for stype in ("ci8", "ci16", "ci32"):
            p = tmp / f"hint_{n}_{stype}.raw"
            w = Writer(p, file_type="raw", sample_type=stype, fs=FS)
            w.write(np.full(n, 0.5 + 0.25j, np.complex64))
            w.close()
            rd = Reader(p)  # no hint: the documented default
            tb = int(rd.trailing_bytes)
            total += 1
            if tb == 0:
                silent += 1
            rows.append(
                [
                    str(n),
                    stype,
                    rd.sample_type,
                    str(int(rd.num_samples)),
                    str(tb),
                    "yes" if tb else "**no**",
                ]
            )
    d.hint_rows = rows
    d.hint_silent, d.hint_total = silent, total
    d.hint_all_wrong_type = all(r[2] == "cf32" for r in rows)
    R.table(
        [
            "samples written",
            "written as",
            "read as",
            "samples read",
            "trailing bytes",
            "tell fires?",
        ],
        rows,
    )
    R.md(
        f"The reader reports `cf32` every time, and the documented "
        f"safeguard -- `trailing_bytes`, which the docstring names for "
        f"exactly this -- stays silent in {silent} of {total} cases. It "
        "can only fire when the byte count fails to divide by 8, so every "
        "even sample count is invisible. **ci32 is invisible at every "
        "length**: it is 8 bytes per complex sample, the same as cf32, so "
        "the count comes back correct and every value is wrong. No "
        "size-based check could ever separate those two."
    )
    R.md()
    d.fs_lost = float(Reader(tmp / "hint_512_ci16.raw").fs)
    R.md(
        f"The sample rate goes the same way: the sidecar records "
        f"`core:sample_rate` = {FS:.0f}, and the reader reports "
        f"{d.fs_lost:.1f}. Filed as gh-1120."
    )
    R.md()


def measure_reach(d: Data) -> None:
    R.md("### 2.6 What the Python face does not reach")
    R.md()
    d.unreachable = [
        (
            "`wfm_writer_open` -- the `FILE*`-taking constructor. Python "
            "gets `wfm_writer_create`, which owns the file; a caller who "
            "already has a stream is a C caller."
        ),
        (
            "`wfm_writer_set_gain` -- reachable, but only as the "
            "`headroom` dB argument at construction (§2.4), never as a "
            "linear gain and never mid-capture."
        ),
        (
            "`wfm_writer_peak` -- the linear peak. Python exposes "
            "`peak_dbfs` and `clipped`, both derived from it; the raw "
            "magnitude is C-only."
        ),
        (
            "`wfm_writer_destroy` -- the same function as `close()` under "
            "the binding's name, so Python reaches the behaviour through "
            "`close()` and the C test covers the identity."
        ),
    ]
    for u in d.unreachable:
        R.md(f"- {u}")
    R.md()
    R.md("Each is certified in C instead -- none is a gap in the binding.")
    R.md()


def characterise(tmp: Path) -> Data:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Each heading names the C section "
        "it tracks where there is one; the numbering is this report's own."
    )
    R.md()
    d = Data()
    measure_wire(d, tmp)
    measure_quantisation(d, tmp)
    measure_peak(d, tmp)
    measure_headroom(d, tmp)
    measure_reader(d, tmp)
    measure_reach(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "CONFIRMED",
        "**The quantiser truncates where the library's own converter "
        f"rounds, and it costs {d.penalty_db:.1f} dB** (gh-1117). "
        '`f32_to_i16`\'s header says it "rounds to the nearest integer"; '
        "`wfm_writer_core.c` has its own `(long)(v * scale)` and truncates "
        "toward zero, as do `wfm_sink.c` and `wfm_reader_core.c` -- three "
        "private copies of one conversion, which is the drift class "
        "`CLAUDE.md` names by example. Measured against both models on the "
        "file itself: truncation on every integer type, rms "
        f"{d.trunc_rms:.4f} LSB against {d.round_rms:.4f} rounded, a "
        "factor of exactly 2. An 8-bit capture sits at "
        f"{d.ci8_floor:.1f} dBFS where it could sit at "
        f"{d.ci8_floor_rounded:.1f}. Not fixed here: the round trip is "
        "self-consistent today, so changing it changes bytes in every "
        "committed capture fixture and needs a deliberate pass. Phase 8 "
        "measures; it does not repair.",
    )
    R.find(
        "F2",
        "FIXED",
        "**The three properties the Python face exposes were untested in "
        "C.** `wfm_writer_get_peak_dbfs`, `wfm_writer_get_clipped` and "
        "`wfm_writer_get_clip_fraction` had zero mentions in any C test in "
        "the tree, and so did `wfm_writer_destroy` -- the binding's "
        "fallible destructor, whose non-zero return is what makes "
        "`Writer.close()` raise out of a `with` block. The accessors are "
        "where a derivation hides: one is a log, one is a rule about which "
        "wire types can saturate. Closed by a new section in "
        "`test_wfm_writer_core.c`, pinned to the digit "
        "(`20*log10(0.25) = -12.0411998`) and to -inf rather than 0 dB "
        "before anything is written. Sabotage: 10*log10 for 20, 0 dB for "
        "-inf, `clipped` without its wire-type condition, and `destroy` "
        "always returning success -- all now red.",
    )
    R.find(
        "F3",
        "BY DESIGN",
        "**A float capture reports a peak here and does not through "
        "`StreamSink`, and the sink says it mirrors this object.** "
        "`wfm_writer`'s header: floats \"never clip but still report a "
        "peak\", and they do (§2.3). `wfm_sink.h`'s clip block opens "
        '"mirroring wfm_writer" and then excludes the float paths '
        'entirely -- "the cf32 path is left untouched ... it never clips '
        'and is the streaming hot path" -- so a caller who moves a '
        "pipeline from a file capture to a stream loses the peak reading "
        "with nothing to say so. Both behaviours are deliberate and both "
        'are documented; what is wrong is the word "mirroring", which '
        "promises a parity that does not exist. Recorded rather than "
        "changed: the streaming hot path has a real reason to skip the "
        "compare.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "**Four header entry points are not on the Python face** (§2.6), "
        "and one of them is worth naming: `wfm_writer_set_gain` is "
        "reachable only as the `headroom` argument at construction, in dB. "
        "A caller cannot change the gain mid-capture from Python, which is "
        "the right default -- a level that moves partway through a file is "
        "a file whose samples mean two different things -- but it means "
        "the linear-gain path is exercised in C alone.",
    )
    R.find(
        "F5",
        "CONFIRMED",
        "**The writer records the sample type in a sidecar the reader does "
        "not read, and the documented safeguard cannot see it** "
        "(gh-1120). Every raw capture gets a `<path>.sigmf-meta` beside it "
        "carrying `core:datatype` and `core:sample_rate` -- the writer's "
        "own comment says the sidecar exists because raw and CSV "
        'otherwise "hand back a file nobody could interpret". `Reader` '
        "opens the same path and does not look: it reports `cf32`, "
        f"`fs = {d.fs_lost:.1f}` against the {FS:.0f} recorded beside it, "
        "half the samples, and garbage values, with nothing raised. "
        "`Reader`'s docstring points at `trailing_bytes` for a wrong hint; "
        f"measured, it stays silent in {d.hint_silent} of "
        f"{d.hint_total} cases, because it can only fire when the byte "
        "count fails to divide by 8 -- so every even sample count is "
        "invisible. For ci32 it can NEVER fire: 8 bytes per complex "
        "sample, the same as cf32, so the count is right and every value "
        "is wrong (§2.5). Passing `sample_type=` works and is the current "
        "contract; the defect is that the answer is already on disk and "
        "getting it wrong is silent.",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits -- the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/wfm/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.wire_all_exact,
        "all five wire types land exactly where the arithmetic says, read "
        "back by numpy at the declared dtype",
    )
    R.limit(
        d.all_truncate and not d.any_round,
        "the integer quantiser truncates toward zero on every type -- "
        "recorded so gh-1117 cannot be 'fixed' without this report moving",
    )
    R.limit(
        abs(d.penalty_db - 6.02) < 0.2,
        f"and that costs {d.penalty_db:.2f} dB against round-to-nearest, "
        "the factor of 2 in quantiser rms the theory predicts",
    )
    R.limit(
        d.worst_lsb <= 1.0 + 1e-9,
        f"the quantisation error never exceeds one LSB "
        f"({d.worst_lsb:.3f} measured)",
    )
    R.limit(
        d.peak_exact,
        "peak_dbfs is 20*log10 of the largest axis written, exactly, at "
        "every level measured",
    )
    R.limit(
        d.clipped_rule,
        "clipped is true for the three integer wire types and false for "
        "the two float ones at the same 1.5 amplitude -- it is a rule "
        "about the wire type, not only the level",
    )
    R.limit(
        d.clip_exact,
        "clip_fraction counts COMPONENTS: 0, exactly one half when only I "
        "saturates, and 1 when both do",
    )
    R.limit(
        d.fraction_off_is_zero,
        "and it is opt-in -- off by default a fully saturated block still "
        "reports 0.0 while clipped reads true",
    )
    R.limit(
        d.remedy_clears,
        "ceil(20*log10(peak)) dB of headroom clears the clip at every "
        "peak measured",
    )
    R.limit(
        d.remedy_bites,
        "and one dB less does not -- the header's 'exactly' is a bound "
        "that bites, not a comfortable margin",
    )
    R.limit(
        d.snr_invariant_db < 1e-6,
        f"headroom moves the level and not the ratio: SNR varies by "
        f"{d.snr_invariant_db:.1e} dB across 12 dB of backoff",
    )
    R.limit(
        d.unity_identical,
        "headroom 0 is a bit-exact no-op -- byte-identical to omitting it, "
        "on all five wire types",
    )
    R.limit(
        d.reader_agrees,
        "Reader recovers the written samples across blue, csv and raw -- "
        "raw only when told the type",
    )
    R.limit(
        d.hint_all_wrong_type and d.hint_silent >= 6,
        f"an untold raw capture reads back as cf32 with trailing_bytes "
        f"silent in {d.hint_silent}/{d.hint_total} cases -- recorded so "
        "gh-1120 cannot be closed without this report moving",
    )
    R.limit(
        d.fs_lost == 0.0,
        "and the sample rate the sidecar records comes back as 0.0",
    )
    R.limit(
        len(d.unreachable) == 4,
        "four header entry points are not on the Python face and are "
        "certified in C instead -- counted, so one appearing or vanishing "
        "is a change",
    )


# ── plots ────────────────────────────────────────────────────────────
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7, 4))
    names = [r[0] for r in d.quant_table]
    xs = np.arange(len(names))
    ax.bar(
        xs - 0.2,
        [float(r[5]) for r in d.quant_table],
        0.4,
        label="as written (truncated)",
    )
    ax.bar(
        xs + 0.2, [float(r[6]) for r in d.quant_table], 0.4, label="if rounded"
    )
    for i, r in enumerate(d.quant_table):
        ax.annotate(
            f"{float(r[5]) - float(r[6]):+.1f} dB",
            (i, float(r[5])),
            ha="center",
            va="bottom",
            fontsize=8,
        )
    ax.set_xticks(xs)
    ax.set_xticklabels(names)
    ax.set_ylabel("quantisation noise floor (dBFS)")
    ax.set_title("Truncating costs 6 dB of the wire, on every integer type")
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "quantisation.png", dpi=110)
    plt.close(fig)


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report.

    `write=False` is the pytest path: every measurement still runs, so
    every limit is genuinely exercised, but nothing is written into the
    repo. The captures themselves always go to a temporary directory --
    a validator must never leave a file in the tree, and these are
    megabytes of them.
    """
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    with tempfile.TemporaryDirectory(prefix="dp-writer-cert-") as td:
        d = characterise(Path(td))
        review(d)
        limits(d)
        R.executive(
            "Writer",
            [
                "**An integer capture costs "
                f"{d.penalty_db:.1f} dB more quantisation noise than it "
                "needs to.** The writer truncates where doppler's own "
                "`f32_to_i16` rounds -- three private copies of that "
                "conversion exist. It bites hardest at 8 bits: "
                f"{d.ci8_floor:.1f} dBFS against a possible "
                f"{d.ci8_floor_rounded:.1f} (§2.2, F1, gh-1117).",
                "**Set headroom from the peak you measured, and the rule "
                "is exact.** `ceil(20*log10(peak))` dB clears the clip at "
                "every level measured, and one dB less does not -- so it "
                "is a bound to design to, not a margin to pad (§2.4).",
                "**Headroom costs no SNR.** A single scale moves signal "
                "and noise together; the ratio varied by "
                f"{d.snr_invariant_db:.0e} dB across 12 dB of backoff. And "
                "headroom 0 is byte-identical to omitting it, so the "
                "no-op really is one (§2.4).",
                "**`clipped` is about the wire type, not the level.** The "
                "same 1.5 that saturates ci16 is merely loud in cf32, and "
                "a float capture still reports a peak -- which "
                "`StreamSink`, whose own comment says it mirrors this "
                "object, does not (§2.3, F3).",
                "**Tell `Reader` the sample type of a raw capture.** "
                "The writer records it in a `.sigmf-meta` sidecar and the "
                "reader does not read it, so an untold raw capture comes "
                "back as cf32 -- half the samples and garbage values, "
                f"with `trailing_bytes` silent in {d.hint_silent} of "
                f"{d.hint_total} cases and unable to fire at all for "
                "ci32. Use BLUE or SigMF if you want the file to say what "
                "it is (§2.5, F5, gh-1120).",
                "**Turn the clip counter on before you need it.** The peak "
                "is always tracked and free; the per-component fraction is "
                "opt-in, and off by default a fully saturated capture "
                "reports 0.0 (§2.3).",
                "**The evidence is younger than the object.** Every "
                "property the Python face exposes -- peak_dbfs, clipped, "
                "clip_fraction -- and the destructor whose status makes "
                "`close()` raise were tested by nothing in C until this "
                "certification (F2).",
            ],
        )
        if write:
            plots(d)
    R.summary(
        "\n- Raw sweeps: `data/quantisation.csv`, `data/headroom_snr.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
