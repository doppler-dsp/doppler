"""Certify `PSD` — the averaging power-spectral-density estimator.

Run:  python -m doppler.spectral.tests.validation.psd.validate
      make validate          (regenerates every report)
      make validate-check    (fails if the committed report is stale)

`PSD` windows, zero-pads and transforms each frame, folds its power into a
running average, and reads that average back as a spectrum (dB, dB/Hz, linear
two- or one-sided) or as a measurement (band power, occupied bandwidth, noise
floor, SNR, SFDR). Its per-frame kernel is what the Spectrogram composes
(#1894), so "a row is that frame's PSD" stands on this report.

The external truth throughout is **numpy's FFT** of the same float32 frame
through the same window computed in double from its published definition —
an implementation that shares nothing with doppler's but the input — plus the
closed forms the header states (ENBW, the dB/Hz offset, Parseval). Statistical
sections size every bound from the estimator's own spread at z = 5, the same
derivations `native/tests/test_psd_core.c` carries beside its assertions.
"""

from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.spectral import PSD
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
R = Report()

SEED = 20261009
WINDOWS = ("hann", "kaiser", "blackman-harris", "rect")
BETA = 8.0
MODES = ("mean", "exp", "maxhold", "minhold")
U = 2.0**-24  # float32 unit roundoff
FFT_C = 5.0  # radix-2 FFT error constant (Higham, sec. 24.1)
# Blackman-Harris, 4-term minimum, published (Harris 1978, Table 1).
BH = (0.35875, 0.48829, 0.14128, 0.01168)


@dataclass
class Data:
    """Everything §3 and §4 read, measured once in §2."""

    level_rows: list[list[str]] = field(default_factory=list)
    level_worst_db: float = 0.0
    layout_exact: bool = False
    bits_is_full_scale: bool = False
    fs_tone_db: float = 0.0
    fft_rows: list[list[str]] = field(default_factory=list)
    fft_within: bool = False
    enbw_rows: list[list[str]] = field(default_factory=list)
    enbw_worst: float = 0.0
    rect_enbw_one: bool = False
    rbw_is_enbw: bool = False
    mode_rows: list[list[str]] = field(default_factory=list)
    mode_within: bool = False
    modes_differ: bool = False
    reset_reseeds: bool = False
    stat_rows: list[list[str]] = field(default_factory=list)
    stat_worst_z: float = 0.0
    dbhz_offset_worst: float = 0.0
    obw_tone_one_bin: bool = False
    obw_noise_bins: float = 0.0
    nf_dev: float = 0.0
    nf_tol: float = 0.0
    nf_median_dev: float = 0.0
    nf_median_tol: float = 0.0
    snr_dev: float = 0.0
    snr_tol: float = 0.0
    sfdr: float = 0.0
    sfdr_one_tone: float = -1.0
    herm_worst: float = 0.0
    herm_bound: float = 0.0
    fold_exact: bool = False
    fold_complex_ok: bool = False
    empty_rows: list[list[str]] = field(default_factory=list)
    empty_ok: bool = False
    linear_ignores_fs: bool = False
    state_exact: bool = False
    state_rejects: bool = False
    # §2.10, the candidate findings (a)-(f), measured
    a_len_padded: int = 0
    a_nfft: int = 0
    a_max_out: int = 0
    b_pad0_nfft: int = 0
    b_pad0_refused: bool = False
    c_alpha_rows: list[list[str]] = field(default_factory=list)
    c_fraction_rows: list[list[str]] = field(default_factory=list)
    c_alpha_refused: bool = False
    c_fraction_nan: bool = False
    d_zero_floor: float = 0.0
    d_snr_outside: float = 0.0
    e_rows: list[list[str]] = field(default_factory=list)
    f_rows: list[list[str]] = field(default_factory=list)
    f_created: bool = False
    g_band_empty: object = None
    i_overflow_refused: bool = False


def _rng(tag: int) -> np.random.Generator:
    return np.random.default_rng(SEED + tag)


def _cnoise(r: np.random.Generator, n: int, var: float) -> np.ndarray:
    """Complex white Gaussian noise, E|x|^2 = var, as complex64."""
    s = math.sqrt(var / 2.0)
    return (s * (r.standard_normal(n) + 1j * r.standard_normal(n))).astype(
        np.complex64
    )


def _tone(n: int, k: float, amp: float = 1.0) -> np.ndarray:
    i = np.arange(n, dtype=np.float64)
    return (amp * np.exp(2j * np.pi * k * i / n)).astype(np.complex64)


def _window(name: str, n: int, beta: float = BETA) -> np.ndarray:
    """The window from its published definition, in double (the truth)."""
    k = np.arange(n, dtype=np.float64)
    if name == "rect":
        return np.ones(n)
    if name == "kaiser":
        return np.kaiser(n, beta)
    x = 2.0 * np.pi * k / (n - 1)  # symmetric: doppler divides by N-1
    if name == "hann":
        return 0.5 - 0.5 * np.cos(x)
    a0, a1, a2, a3 = BH
    return a0 - a1 * np.cos(x) + a2 * np.cos(2 * x) - a3 * np.cos(3 * x)


def _nfft(n: int, pad: int) -> int:
    return 1 << max(0, (n * max(pad, 1) - 1).bit_length())


def _truth_power(x: np.ndarray, w: np.ndarray, nfft: int) -> np.ndarray:
    """numpy's DC-centred |FFT|^2 of one windowed, zero-padded frame."""
    xw = w * x.astype(np.complex128)
    return np.abs(np.fft.fftshift(np.fft.fft(xw, nfft))) ** 2


def _w4_ratio(w: np.ndarray) -> float:
    w2 = w * w
    return float(np.sum(w2 * w2) / np.sum(w2) ** 2)


def _fft_bound(nfft: int) -> float:
    """A bin's power error, as a fraction of the frame's total power."""
    return 2.0 * FFT_C * math.log2(nfft) * U


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    # A limits-only run (pytest's `build(write=False)`) must not write.
    if not R.write:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def _psd(**kw) -> PSD:
    kw.setdefault("fs", 1.0)
    kw.setdefault("beta", BETA)
    return PSD(**kw)


def _db(w: PSD) -> np.ndarray:
    return np.asarray(w.psd_db(w.psd_db_max_out()), dtype=np.float64)


def _two(w: PSD) -> np.ndarray:
    return np.asarray(
        w.power_twosided(w.power_twosided_max_out()), dtype=np.float64
    )


def _one(w: PSD) -> np.ndarray:
    return np.asarray(
        w.power_onesided(w.power_onesided_max_out()), dtype=np.float64
    )


# ── 1. the object ─────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object — an averaging periodogram and its measurements")
    R.md()
    R.md(
        "`PSD` windows each length-`n` frame, zero-pads it to "
        "`nfft = next_pow_two(n * pad)`, transforms it, and folds `|X|^2` "
        "into a per-bin average (mean, EMA, max-hold or min-hold), stored "
        "DC-centred. It reads the average back as a spectrum and as the "
        "measurements a spectrum analyser makes. Its per-frame kernel, "
        "`dp_psd_frame_power` / `dp_psd_frame_db`, is what the Spectrogram "
        "composes."
    )
    R.md()
    R.table(
        ["page", "owns"],
        [
            [
                "[`docs/guide/spectral-psd.md`]"
                "(../../../../../../docs/guide/spectral-psd.md)",
                "how to drive it: windows, padding, the dBFS reference, the "
                "measurements",
            ],
            [
                "[`docs/design/spectrogram.md`]"
                "(../../../../../../docs/design/spectrogram.md)",
                "the composition this certification exists to support: a "
                "row is that frame's PSD",
            ],
            [
                "`native/inc/doppler/psd/psd_core.h`",
                "the contract per function — the SSOT this report audits",
            ],
        ],
    )
    R.md("### 1.1 The claim inventory")
    R.md()
    R.md(
        "Step 1 of `docs/dev/contributing/validation.md`, against "
        "`test_psd_core.c` as it stood at v0.65.0: 22 entry points, plus "
        "`dp_psd_frame_linear`, added since (#1963). Rows marked **was** "
        "were absent or pinned only at literals and are now the C sections "
        "named (T1–T18, added in two test-only PRs before this report)."
    )
    R.md()
    R.table(
        ["header claim", "pinned where", "here"],
        [
            [
                "one frame through the kernel = accumulate-then-read, bit "
                "for bit",
                "C, 4 windows x n {64, 100} x pad {1, 2}",
                "C-ONLY (F4)",
            ],
            [
                "the kernel does not touch the running average",
                "**was** empty average, `frame_db` only — now C T1",
                "C-ONLY (F4)",
            ],
            [
                "`frame_linear`: 10·log10 of it is `frame_db` wherever "
                "`frame_db` is above the floor",
                "C, the `dp_psd_frame_linear` block (#1963)",
                "C-ONLY (F4)",
            ],
            [
                "`frame_linear`: a full-scale tone on a bin reads 1.0 "
                "whatever the window, padded or not, against `full_scale` "
                "and `bits` alike",
                "C, the same block (#1963)",
                "C-ONLY (F4)",
            ],
            [
                "a full-scale tone on a bin reads 0 dBFS whatever the window",
                "**was** rect only — now C T2",
                "§2.1",
            ],
            [
                "`bits > 0` sets `full_scale = 2^(bits-1)`; `full_scale` "
                "then ignored",
                "**was nothing** in C — now T2",
                "§2.1",
            ],
            [
                "DC-centred: bin k at `nfft/2 + k*nfft/n`",
                "**was** positive k, `nfft == n` — now T4",
                "§2.1",
            ],
            ["the -200 dB floor", "**was nothing** — now T3", "§2.8"],
            [
                "ENBW: rect 1, Hann ~1.5, B-H published",
                "**was** Hann/Kaiser only — now T5",
                "§2.3",
            ],
            [
                "`nfft = next_pow_two(n * pad)`",
                "**was nothing** — now T18",
                "§2.1",
            ],
            [
                "the four averaging modes",
                "**was** `max >= min`, which two mean states pass — now T9",
                "§2.4",
            ],
            [
                "reset discards the average",
                "**was** mean mode on identical frames — now T10",
                "§2.4",
            ],
            [
                "dB/Hz is `fs * s2`-normalised; offset "
                "`10 log10(cg^2/(fs s2))`",
                "**was** 'a constant' — now T11",
                "§2.5",
            ],
            [
                "band power is absolute: window- and pad-invariant",
                "C windows 0-2; **rect and noise were nothing** — now T12",
                "§2.5",
            ],
            [
                "occupied bandwidth holds `fraction` of the power",
                "**was** `0 < obw < 0.5` — now T13",
                "§2.6",
            ],
            [
                "noise floor is the MEDIAN of the dB spectrum; SNR; SFDR",
                "**was** `isfinite` / `> 0` — now T14, T14b, T15, T16",
                "§2.6",
            ],
            [
                "real input: Hermitian, one-sided fold, DC/Nyquist kept",
                "**was** the energy sum only — now T8",
                "§2.7",
            ],
            [
                "readers return 0 (Python None) before any frame",
                "**was** `psd_db` only — now T6",
                "§2.8",
            ],
            [
                "linear power does NOT apply `full_scale`",
                "**was nothing** — now T7",
                "§2.8",
            ],
            [
                "`beta` shapes only Kaiser; `n >= 2`",
                "**was nothing** — now T17",
                "§2.3",
            ],
            ["the state triplet round-trips", "C + Python", "§2.9"],
        ],
    )


# ── 2. characterisation ───────────────────────────────────────────────


def characterise() -> Data:
    d = Data()
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured through the binding, against numpy's FFT and the closed "
        "forms. No verdicts here; §3 judges."
    )
    R.md()
    _sec_levels(d)
    _sec_fft(d)
    _sec_enbw(d)
    _sec_modes(d)
    _sec_density(d)
    _sec_measurements(d)
    _sec_real(d)
    _sec_contracts(d)
    _sec_state(d)
    _sec_candidates(d)
    return d


def _sec_levels(d: Data) -> None:
    R.md("### 2.1 Where a tone lands, and at what level (C T2, T4, T18)")
    R.md()
    rows: list[list[str]] = []
    worst = 0.0
    exact = True
    n = 64
    for win in WINDOWS:
        for pad in (1, 2, 4):
            lv = 0.0
            for k in (-7, 0, 5):
                w = _psd(n=n, window=win, pad=pad)
                exact &= w.nfft == _nfft(n, pad)
                w.accumulate(_tone(n, k))
                db = _db(w)
                at = w.nfft // 2 + k * (w.nfft // n)
                exact &= int(np.argmax(db)) == at
                lv = max(lv, abs(float(db[at])))
            worst = max(worst, lv)
            rows.append([win, str(pad), str(_nfft(n, pad)), f"{lv:.2e}"])
    R.table(["window", "pad", "nfft", "worst bin-tone level, dB"], rows)
    d.level_rows, d.level_worst_db, d.layout_exact = rows, worst, exact

    fs_amp = 2.0**11
    wf = _psd(n=n, window="hann", full_scale=fs_amp)
    wb = _psd(n=n, window="hann", bits=12, full_scale=999.0)
    x = _tone(n, 5, fs_amp)
    wf.accumulate(x)
    wb.accumulate(x)
    a, b = _db(wf), _db(wb)
    d.bits_is_full_scale = wb.full_scale == fs_amp and np.array_equal(a, b)
    d.fs_tone_db = float(a[n // 2 + 5])
    R.md(
        f"Bin-centred unit tones at k in {{-7, 0, 5}}: every peak lands at "
        f"`nfft/2 + k*nfft/n` and `nfft` is `next_pow_two(n*pad)` in all "
        f"{len(rows) * 3} cases ({'yes' if exact else 'NO'}); the worst "
        f"level is {worst:.2e} dB from 0 dBFS. A tone of amplitude 2^11 "
        f"reads {d.fs_tone_db:+.2e} dB against `full_scale = 2^11`, and "
        f"`bits = 12` (with `full_scale = 999` passed and ignored) gives the "
        f"same bytes: {'yes' if d.bits_is_full_scale else 'NO'}."
    )
    R.md()


def _sec_fft(d: Data) -> None:
    R.md("### 2.2 One frame against numpy's FFT (C kernel equality, T9)")
    R.md()
    R.md(
        "A random complex frame through each window and pad, compared bin by "
        "bin with numpy's double-precision transform of the same float32 "
        "samples. The error is a fraction of the frame's total power, "
        "bounded by `2 c log2(nfft) u` (Higham sec. 24.1, c = 5)."
    )
    R.md()
    rows: list[list[str]] = []
    within = True
    csv = []
    for n in (64, 100):
        for win in WINDOWS:
            for pad in (1, 2):
                x = _cnoise(_rng(10 + n + pad), n, 1.0)
                w = _psd(n=n, window=win, pad=pad)
                w.accumulate(x)
                ww = _window(win, n)
                truth = _truth_power(x, ww, w.nfft) / np.sum(ww) ** 2
                err = float(np.max(np.abs(_two(w) - truth)) / np.sum(truth))
                bound = _fft_bound(w.nfft)
                within &= err <= bound
                rows.append(
                    [str(n), win, str(pad), f"{err:.2e}", f"{bound:.2e}"]
                )
                csv.append([n, WINDOWS.index(win), pad, err, bound])
    R.table(["n", "window", "pad", "worst bin error / total", "bound"], rows)
    _csv(HERE / "data" / "fft_vs_numpy.csv", "n,window,pad,err,bound", csv)
    d.fft_rows, d.fft_within = rows, within


def _sec_enbw(d: Data) -> None:
    R.md("### 2.3 ENBW and the window (C T5, T17)")
    R.md()
    rows: list[list[str]] = []
    worst = 0.0
    a0, a1, a2, a3 = BH
    p = a0 * a0 + (a1 * a1 + a2 * a2 + a3 * a3) / 2.0
    w0 = a0 - a1 + a2 - a3
    rect_one = True
    rbw_ok = True
    for n in (64, 100, 1024):
        for win in WINDOWS:
            w = _psd(n=n, window=win, fs=2.0)
            ww = _window(win, n)
            truth = n * float(np.sum(ww * ww)) / float(np.sum(ww)) ** 2
            if win == "blackman-harris":
                closed = n * ((n - 1) * p + w0 * w0) / ((n - 1) * a0 + w0) ** 2
            else:
                closed = truth
            err = abs(w.enbw - closed)
            worst = max(worst, err)
            if win == "rect":
                rect_one &= w.enbw == 1.0
            rbw_ok &= abs(w.rbw - w.enbw * w.fs / n) <= 1e-12 * w.rbw
            rows.append([str(n), win, f"{w.enbw:.6f}", f"{err:.1e}"])
    R.table(["n", "window", "ENBW, bins", "|ENBW - truth|"], rows)
    R.md(
        f"The truth is `n sum(w^2) / sum(w)^2` of the window built in double "
        f"from its definition (Blackman-Harris: the closed form from Harris's "
        f"coefficients for the symmetric N-point window, whose periodic limit "
        f"{p / (a0 * a0):.4f} his Table 1 rounds to 2.00). Worst "
        f"{worst:.1e}. Rect is exactly 1.0: {'yes' if rect_one else 'NO'}; "
        f"`rbw = enbw * fs / n`: {'yes' if rbw_ok else 'NO'}."
    )
    R.md()
    d.enbw_rows, d.enbw_worst = rows, worst
    d.rect_enbw_one, d.rbw_is_enbw = rect_one, rbw_ok


def _sec_modes(d: Data) -> None:
    R.md("### 2.4 The averaging modes, and reset (C T9, T10)")
    R.md()
    n, alpha = 64, 0.25
    r = _rng(20)
    scales = (1.0, 3.0, 0.5, 2.0, 0.25, 1.5)
    frames = [(s * _cnoise(r, n, 1.0)).astype(np.complex64) for s in scales]
    ww = _window("hann", n)
    cg2 = float(np.sum(ww)) ** 2
    pw = [_truth_power(f, ww, n) / cg2 for f in frames]
    truth = {
        "mean": np.mean(pw, axis=0),
        "maxhold": np.max(pw, axis=0),
        "minhold": np.min(pw, axis=0),
    }
    y = pw[0].copy()
    for p in pw[1:]:
        y = y + alpha * (p - y)
    truth["exp"] = y
    rows: list[list[str]] = []
    within = True
    for m in MODES:
        w = _psd(n=n, window="hann", mode=m, alpha=alpha)
        for f in frames:
            w.accumulate(f)
        err = float(np.max(np.abs(_two(w) - truth[m])) / np.sum(truth[m]))
        bound = _fft_bound(n)
        within &= err <= bound
        rows.append([m, f"{err:.2e}", f"{bound:.2e}"])
    R.table(["mode", "worst bin error / total", "bound"], rows)
    distinct = [truth[m] for m in MODES]
    d.modes_differ = all(
        not np.allclose(distinct[i], distinct[j], rtol=1e-3)
        for i in range(4)
        for j in range(i + 1, 4)
    )
    reseed = True
    for m in ("maxhold", "exp"):
        fresh = _psd(n=n, window="hann", mode=m, alpha=alpha)
        fresh.accumulate(frames[2])
        w = _psd(n=n, window="hann", mode=m, alpha=alpha)
        w.accumulate((10.0 * frames[0]).astype(np.complex64))
        loud = _two(w)
        w.reset()
        w.accumulate(frames[2])
        reseed &= not np.array_equal(loud, _two(fresh))
        reseed &= w.count == 1 and np.array_equal(_two(w), _two(fresh))
    R.md(
        f"Six frames at six levels, against the defining rule of each mode "
        f"applied to numpy's per-frame powers (exp: `y1 = P1, y += alpha (P - "
        f"y)`, alpha = {alpha}). The four truths differ from each other: "
        f"{'yes' if d.modes_differ else 'NO'}, so each row is its own "
        f"question. Reset in maxhold and exp — a loud frame, reset, a quiet "
        f"one — reads exactly as a fresh state given the quiet one: "
        f"{'yes' if reseed else 'NO'}."
    )
    R.md()
    d.mode_rows, d.mode_within, d.reset_reseeds = rows, within, reseed


def _sec_density(d: Data) -> None:
    R.md("### 2.5 Density and band power as absolutes (C T11, T12)")
    R.md()
    R.md(
        "Complex white noise of variance `var` at `fs`. By Parseval the mean "
        "over bins of `|X|^2` is the frame's windowed energy "
        "`sum w^2 |x|^2`, so averaged over K frames it estimates `var s2` "
        "with relative sd `sqrt(sum w^4 / (sum w^2)^2 / K)`. dB/Hz should "
        "read `var / fs`; whole-span band power `var`. z = 5."
    )
    R.md()
    k_frames, n, var, fs = 1024, 64, 4.0, 2.0
    rows: list[list[str]] = []
    worst_z = 0.0
    off_worst = 0.0
    csv = []
    for win in WINDOWS:
        for pad in (1, 2):
            r = _rng(30 + 10 * WINDOWS.index(win) + pad)
            w = _psd(n=n, window=win, pad=pad, fs=fs)
            for _ in range(k_frames):
                w.accumulate(_cnoise(r, n, var))
            sigma = math.sqrt(_w4_ratio(_window(win, n)) / k_frames)
            hz = np.asarray(w.psd_dbhz(w.psd_dbhz_max_out()), np.float64)
            db = _db(w)
            dens = float(np.mean(10.0 ** (hz / 10.0)))
            z_hz = (dens / (var / fs) - 1.0) / sigma
            bp = w.total_band_power(np.array([-fs / 2, fs / 2]))
            z_bp = (10.0 ** (bp / 10.0) / var - 1.0) / sigma
            ww = _window(win, n)
            want_off = 10.0 * math.log10(
                float(np.sum(ww)) ** 2 / (fs * float(np.sum(ww * ww)))
            )
            off_worst = max(
                off_worst, float(np.max(np.abs(hz - db - want_off)))
            )
            worst_z = max(worst_z, abs(z_hz), abs(z_bp))
            tol = 10.0 * math.log10(1.0 + 5.0 * sigma)
            rows.append(
                [
                    win,
                    str(pad),
                    f"{10 * math.log10(dens / (var / fs)):+.3f}",
                    f"{bp - 10 * math.log10(var):+.3f}",
                    f"{tol:.3f}",
                    f"{z_hz:+.2f} / {z_bp:+.2f}",
                ]
            )
            csv.append([WINDOWS.index(win), pad, z_hz, z_bp, tol])
    R.table(
        [
            "window",
            "pad",
            "dB/Hz - var/fs, dB",
            "band - var, dB",
            "tol (5 sd), dB",
            "z (dB/Hz / band)",
        ],
        rows,
    )
    _csv(HERE / "data" / "density.csv", "window,pad,z_hz,z_band,tol_db", csv)
    R.md(
        f"K = {k_frames}, n = {n}. Worst |z| {worst_z:.2f}. The dB/Hz - dB "
        f"offset matches `10 log10(cg^2 / (fs s2))` to {off_worst:.1e} dB in "
        f"every bin."
    )
    R.md()
    d.stat_rows, d.stat_worst_z, d.dbhz_offset_worst = rows, worst_z, off_worst


def _sec_measurements(d: Data) -> None:
    R.md(
        "### 2.6 Occupied bandwidth, noise floor, SNR, SFDR (C T13-T16, T14b)"
    )
    R.md()
    t = _psd(n=64, window="rect")
    t.accumulate(_tone(64, 7))
    d.obw_tone_one_bin = t.occupied_bw(0.99) == 1.0 / 64

    nn, k_frames = 1024, 256
    r = _rng(40)
    w = _psd(n=nn, window="rect")
    for _ in range(k_frames):
        w.accumulate(_cnoise(r, nn, 1.0))
    d.obw_noise_bins = w.occupied_bw(0.99) * nn

    n, k_frames, var = 256, 256, 1.0
    med_bias = 10.0 * math.log10(1.0 - 1.0 / (3.0 * k_frames))
    floor_sd = math.sqrt(math.pi / 2.0) / math.sqrt(n * k_frames)
    cross_sd = math.sqrt(2.0 * var / n) / math.sqrt(k_frames)
    r = _rng(41)
    s = _tone(n, 40)
    w = _psd(n=n, window="rect")
    for _ in range(k_frames):
        w.accumulate((_cnoise(r, n, var) + s).astype(np.complex64))
    d.nf_dev = w.noise_floor() - (10.0 * math.log10(var / n) + med_bias)
    d.nf_tol = 10.0 * math.log10(1.0 + 5.0 * floor_sd)
    snr_want = 10.0 * math.log10(1.0 + n / var) - med_bias
    d.snr_dev = w.snr(37 / n, 43 / n) - snr_want
    d.snr_tol = 10.0 * math.log10(
        1.0 + 5.0 * math.sqrt(floor_sd**2 + cross_sd**2)
    )

    # the median, not a mean: 32 of 256 bins carry a tone (T14b)
    r = _rng(42)
    tones = sum(_tone(n, 8 * j - 124).astype(np.complex128) for j in range(32))
    w = _psd(n=n, window="rect")
    for _ in range(k_frames):
        w.accumulate((_cnoise(r, n, var) + tones).astype(np.complex64))
    pq, zq, phi = 128.5 / 224.0, 0.1857, 0.3921
    c = 1.0 / (9.0 * k_frames)
    q = (1.0 - c + zq * math.sqrt(c)) ** 3
    sd = math.sqrt(pq * (1.0 - pq) / 224.0) / phi / math.sqrt(k_frames)
    d.nf_median_dev = w.noise_floor() - (
        10.0 * math.log10(var / n) + 10.0 * math.log10(q)
    )
    d.nf_median_tol = 10.0 * math.log10(1.0 + 5.0 * sd)

    w = _psd(n=64, window="hann")
    w.accumulate((_tone(64, -16) + 0.1 * _tone(64, 16)).astype(np.complex64))
    d.sfdr = w.sfdr(-30.0)
    o = _psd(n=64, window="hann")
    o.accumulate(_tone(64, -16))
    d.sfdr_one_tone = o.sfdr(-30.0)

    R.table(
        ["measurement", "configuration", "measured vs truth", "bound"],
        [
            [
                "OBW, tone",
                "rect, bin-centred",
                "1 bin" if d.obw_tone_one_bin else "NOT one bin",
                "exactly fs / nfft",
            ],
            [
                "OBW(0.99), flat noise",
                "rect, nfft 1024, K 256",
                f"{d.obw_noise_bins:.0f} bins",
                "[1012, 1016] (flat grid 1014; an edge steps out at ~1 sd)",
            ],
            [
                "noise floor",
                "rect, n 256, K 256",
                f"{d.nf_dev:+.4f} dB",
                f"{d.nf_tol:.3f} dB (5 sd of a median of Gamma(K))",
            ],
            [
                "noise floor, 32 tones on 1/8 of the bins",
                "the same",
                f"{d.nf_median_dev:+.4f} dB",
                f"{d.nf_median_tol:.3f} dB — a mean of dB would read +3 dB",
            ],
            [
                "SNR",
                "unit tone + var 1",
                f"{d.snr_dev:+.4f} dB",
                f"{d.snr_tol:.3f} dB (floor sd + tone x noise cross term)",
            ],
            [
                "SFDR",
                "0 and -20 dB tones, N/2 apart, Hann",
                f"{d.sfdr:.6f} dB",
                "20 +- 0.002",
            ],
            [
                "SFDR, one tone",
                "the carrier alone",
                f"{d.sfdr_one_tone:.1f}",
                "exactly 0 (fewer than two peaks)",
            ],
        ],
    )
    R.md(
        "The SFDR tones sit half the transform apart on purpose. At bins 6 "
        "and 20 the carrier's -82 dBc symmetric-Hann leakage moves the spur "
        "by 5.1e-3 dB — a double-precision model of this readout reproduced "
        "it to 1e-6 dB before the layout changed. N/2 apart, each tone's "
        "leakage is symmetric about the other and the bias vanishes."
    )
    R.md()


def _sec_real(d: Data) -> None:
    R.md("### 2.7 Real input and the one-sided fold (C T8)")
    R.md()
    n = 64
    r = _rng(50)
    x = r.standard_normal(2 * n + 7).astype(np.float32)
    w = _psd(n=n, window="hann")
    w.accumulate_real(x)
    two, one = _two(w), _one(w)
    h = n // 2
    d.herm_worst = float(
        np.max(np.abs(two[h + 1 : n] - two[h - 1 : 0 : -1])) / np.sum(two)
    )
    d.herm_bound = _fft_bound(n)
    interior = two[h + 1 : n] + two[h - 1 : 0 : -1]
    d.fold_exact = (
        w.count == 2
        and one[0] == two[h]
        and one[h] == two[0]
        and bool(np.all(np.abs(one[1:h] - interior) <= 4 * U * interior))
    )
    c = _psd(n=n, window="hann")
    c.accumulate(_cnoise(r, n, 1.0))
    t2, o1 = _two(c), _one(c)
    halves = t2[h + 1 : n] + t2[h - 1 : 0 : -1]
    asym = bool(
        np.any(np.abs(t2[h + 1 : n] - t2[h - 1 : 0 : -1]) > 0.1 * halves)
    )
    d.fold_complex_ok = asym and bool(
        np.all(np.abs(o1[1:h] - halves) <= 4 * U * halves)
    )
    R.md(
        f"Two real frames and a 7-sample tail: {w.count} frames taken. +k and "
        f"-k agree to {d.herm_worst:.1e} of the total (bound "
        f"{d.herm_bound:.1e}). DC and Nyquist are kept as-is and every "
        f"interior one-sided bin is the sum of its halves: "
        f"{'yes' if d.fold_exact else 'NO'}. On a complex frame, where the "
        f"halves differ, the fold still sums them: "
        f"{'yes' if d.fold_complex_ok else 'NO'} — the case a real frame "
        f"cannot test (§3 F3)."
    )
    R.md()


def _sec_contracts(d: Data) -> None:
    R.md(
        "### 2.8 The empty contract, the floor, and full_scale (C T3, T6, T7)"
    )
    R.md()
    w = _psd(n=64, window="hann")
    band = np.array([-0.25, 0.25])
    seen = [
        ["psd_db()", repr(w.psd_db(64))],
        ["psd_dbhz()", repr(w.psd_dbhz(64))],
        ["power_twosided()", repr(w.power_twosided(64))],
        ["power_onesided()", repr(w.power_onesided(33))],
        ["band_power(bands)", repr(w.band_power(band))],
        ["total_band_power(bands)", f"{w.total_band_power(band):.4f}"],
        ["occupied_bw(0.99)", repr(w.occupied_bw(0.99))],
        ["noise_floor()", repr(w.noise_floor())],
        ["snr(lo, hi)", repr(w.snr(-0.25, 0.25))],
        ["sfdr(min_db)", repr(w.sfdr(-120.0))],
    ]
    R.table(["reader, before any frame", "returns"], seen)
    vals = [s[1] for s in seen]
    # band_power answered an EMPTY array here, unlike its four siblings;
    # #1959 made it None like them -- (g) in §2.10, F10 in §3.
    d.empty_ok = (
        all(v == "None" for v in vals[:5])
        and abs(float(vals[5]) + 200.0) < 1e-4
        and all(v == "0.0" for v in vals[6:])
    )
    z = _psd(n=64, window="hann")
    z.accumulate(np.zeros(64, np.complex64))
    floor_ok = bool(np.all(np.abs(_db(z) + 200.0) < 1e-4))
    r = _rng(60)
    x = _cnoise(r, 64, 1.0)
    w1, w4 = _psd(n=64, full_scale=1.0), _psd(n=64, full_scale=4.0)
    w1.accumulate(x)
    w4.accumulate(x)
    d.linear_ignores_fs = (
        np.array_equal(_two(w1), _two(w4))
        and np.array_equal(_one(w1), _one(w4))
        and bool(
            np.all(np.abs(_db(w1) - _db(w4) - 20.0 * math.log10(4.0)) < 1e-4)
        )
    )
    d.empty_ok &= floor_ok
    R.md(
        f"An all-zero frame reads the -200 dB floor in every bin: "
        f"{'yes' if floor_ok else 'NO'}. The linear readouts are identical at "
        f"`full_scale` 1 and 4 while `psd_db` moves by exactly 12.04 dB: "
        f"{'yes' if d.linear_ignores_fs else 'NO'}. Note what the scalars "
        f"return when empty — `0.0`, a value they can also return measured "
        f"(§2.10 (d))."
    )
    R.md()


def _sec_state(d: Data) -> None:
    R.md("### 2.9 The state triplet")
    R.md()
    r = _rng(70)
    f = [_cnoise(r, 64, 1.0) for _ in range(3)]
    a = _psd(n=64, window="kaiser", mode="maxhold")
    b = _psd(n=64, window="kaiser", mode="maxhold")
    a.accumulate(f[0])
    a.accumulate(f[1])
    blob = a.get_state()
    b.set_state(blob)
    a.accumulate(f[2])
    b.accumulate(f[2])
    d.state_exact = a.count == b.count and np.array_equal(_db(a), _db(b))
    bad = bytearray(blob)
    bad[0] ^= 0xFF
    try:
        b.set_state(bytes(bad))
        d.state_rejects = False
    except ValueError:
        d.state_rejects = True
    R.md(
        f"Two frames, `get_state`, `set_state` into a fresh object, one more "
        f"frame into both: bit-exact ({'yes' if d.state_exact else 'NO'}). A "
        f"clobbered envelope is refused with `ValueError`: "
        f"{'yes' if d.state_rejects else 'NO'}."
    )
    R.md()


def _sec_candidates(d: Data) -> None:
    R.md("### 2.10 The candidate findings, measured")
    R.md()
    R.md(
        "Six things the inventory flagged as possibly wrong, (a)-(f), one "
        "found measuring them, (g), and one the Spectrogram's review found, "
        "(i). They were first measured before anything was fixed; #1959 then "
        "fixed, at the primitive that owns each rule, what §3 marks FIXED. "
        "These rows measure this tree. Characterisation fixes nothing; §3 "
        "judges."
    )
    R.md()

    # (a) the header's n where the code uses nfft
    w = _psd(n=64, pad=2)
    w.accumulate(_tone(64, 3))
    d.a_len_padded = len(w.psd_db(w.psd_db_max_out()))
    d.a_nfft = w.nfft
    d.a_max_out = w.psd_dbhz_max_out()
    R.md(
        f"**(a)** With `n = 64, pad = 2`: `psd_db` returns "
        f"{d.a_len_padded} values, `nfft` is {d.a_nfft}, `psd_dbhz_max_out()` "
        f"is {d.a_max_out}. The header says `psd_db` writes 'at least n' and "
        f"returns 'min(n, max_out)', and that `psd_dbhz_max_out` 'equals n'. "
        f"Its file comment names two windows ('Hann or Kaiser') of four."
    )
    R.md()

    # (b) pad = 0
    try:
        p0 = _psd(n=64, pad=0)
        d.b_pad0_nfft = p0.nfft
        d.b_pad0_refused = False
    except (ValueError, MemoryError):
        d.b_pad0_refused = True
    R.md(
        "**(b)** `pad = 0`: "
        + (
            "refused."
            if d.b_pad0_refused
            else f"accepted, `nfft = {d.b_pad0_nfft}` — silently `pad = 1`, "
            f"though the header documents `pad (>= 1)`."
        )
    )
    R.md()

    # (c) alpha and fraction, never range-checked
    r = _rng(80)
    frames = [_cnoise(r, 64, s) for s in (1.0, 16.0, 1.0)]
    first = _psd(n=64, mode="mean")
    first.accumulate(frames[0])
    p1 = float(np.mean(_two(first)))
    rows = []
    for alpha in (0.25, 0.0, -0.5, 1.0, 1.5):
        try:
            w = _psd(n=64, mode="exp", alpha=alpha)
            for f in frames:
                w.accumulate(f)
            rows.append(
                [
                    f"{alpha:g}",
                    "accepted",
                    f"{float(np.mean(_two(w))) / p1:+.3g}",
                ]
            )
        except (ValueError, MemoryError) as e:
            rows.append([f"{alpha:g}", f"refused ({type(e).__name__})", "—"])
    d.c_alpha_rows = rows
    d.c_alpha_refused = all(
        row[1].startswith("refused")
        for row in rows
        if float(row[0]) <= 0.0 or float(row[0]) > 1.0
    )
    R.md(
        "**(c)** exp-mode `alpha` over three frames at 1x, 16x and 1x power. "
        "The readout is mean two-sided power over the first frame's: the EMA "
        "at 0.25 gives ~3.8, and 1 keeps only the last frame (1). "
        + (
            "Every alpha outside `(0, 1]` is refused at create — AccTrace's "
            "rule, which PSD reaches by returning its NULL."
            if d.c_alpha_refused
            else "An alpha outside `(0, 1]` is accepted: 0 freezes the first "
            "frame, above 1 keeps only the last, and a negative alpha "
            "extrapolates."
        )
    )
    R.md()
    R.table(["alpha", "create", "mean power / first frame's"], rows)
    w = _psd(n=64, window="rect")
    w.accumulate(_cnoise(_rng(81), 64, 1.0))
    frows = []
    for frac in (0.99, 1.0, 0.0, -1.0, 1.5):
        frows.append([f"{frac:g}", f"{w.occupied_bw(frac):.6g}"])
    d.c_fraction_rows = frows
    d.c_fraction_nan = all(
        math.isnan(float(row[1])) for row in frows if row[0] != "0.99"
    )
    R.md(
        "`occupied_bw(fraction)`, documented for the open interval `(0, 1)`; "
        "the search is `dp_obw_from_power`'s, handed the averager's own "
        "trace:"
    )
    R.md()
    R.table(["fraction", "returns, Hz (fs = 1)"], frows)

    # (d) a 0 return that a measurement can also produce
    n = 256
    w = _psd(n=n, window="rect")
    r = _rng(82)
    for _ in range(256):
        w.accumulate(_cnoise(r, n, float(n)))  # var/n = 1: floor at 0 dB
    d.d_zero_floor = w.noise_floor()
    d.d_snr_outside = w.snr(10.0, 11.0)
    R.md(
        f"**(d)** Noise scaled so its floor is 0 dB reads "
        f"`noise_floor() = {d.d_zero_floor:+.4f}` — next to the empty "
        f"state's `0.0`. `snr` over a band entirely outside the span returns "
        f"{d.d_snr_outside!r}, undocumented, and the same as 'no SNR'."
    )
    R.md()

    # (e) the bin two adjacent bands share
    rows = []
    for win in ("rect", "hann"):
        w = _psd(n=64, window=win)
        w.accumulate(_tone(64, 0))
        whole = w.total_band_power(np.array([-0.5, 0.5]))
        halves = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))
        lin = float(np.sum(10.0 ** (np.asarray(halves) / 10.0)))
        rows.append(
            [
                win,
                f"{whole:+.3f}",
                f"{halves[0]:+.3f}, {halves[1]:+.3f}",
                f"{10 * math.log10(lin):+.3f}",
            ]
        )
    d.e_rows = rows
    R.md(
        "**(e)** A DC tone, read whole and as the two halves `[-fs/2, 0]` and "
        "`[0, fs/2]`, which share the DC bin:"
    )
    R.md()
    R.table(
        ["window", "whole span, dB", "halves, dB", "halves summed, dB"], rows
    )

    # (f) n = 2 Hann: a window with zero coherent gain
    rows = []
    try:
        w = _psd(n=2, window="hann")
        d.f_created = True
        rows.append(["create", "accepted", f"enbw = {w.enbw!r}"])
        w.accumulate(np.array([1.0, 1.0], np.complex64))
        rows.append(["psd_db()", repr(list(_db(w))), ""])
        rows.append(
            ["psd_dbhz()", repr(list(w.psd_dbhz(w.psd_dbhz_max_out()))), ""]
        )
        rows.append(["power_twosided()", repr(list(_two(w))), ""])
        rows.append(
            [
                "total_band_power(whole)",
                repr(w.total_band_power(np.array([-0.5, 0.5]))),
                "",
            ]
        )
        rows.append(["noise_floor()", repr(w.noise_floor()), ""])
    except (ValueError, MemoryError) as e:
        rows.append(["create", f"refused ({type(e).__name__})", ""])
    d.f_rows = rows
    R.md(
        "**(f)** `n = 2` with Hann: the symmetric 2-point Hann is `[0, 0]`, "
        "so `cg = s2 = 0`. Create, then, if it is accepted, a unit frame and "
        "each reader:"
    )
    R.md()
    R.table(["call", "returns", "note"], rows)
    w = _psd(n=64)
    d.g_band_empty = w.band_power(np.array([-0.25, 0.25]))
    R.md(
        "**(g)** Found while measuring §2.8: before any frame, four readers "
        "return `None`, and `band_power` returns "
        f"{d.g_band_empty!r}. The C contract is the same for all five: 0 "
        "values written."
    )
    R.md()

    # (i) n * pad past what a size_t can count in complex bytes
    try:
        _psd(n=1 << 62, pad=1)
        d.i_overflow_refused = False
    except (ValueError, MemoryError, OverflowError):
        d.i_overflow_refused = True
    R.md(
        "**(i)** `n = 2^62`, found by the Spectrogram's review: "
        + (
            "refused before any allocation."
            if d.i_overflow_refused
            else "accepted."
        )
        + " `n * pad` past 2^60 cannot size a complex buffer whose byte "
        "count a 64-bit `size_t` holds."
    )
    R.md()


# ── 3. review ─────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Review — findings")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The kernel's claims were mostly unpinned.** `frame_db`'s 0 dBFS "
        "'whatever the window' was asserted for the rectangular window only, "
        "`bits` and `full_scale != 1` had no C test at all, and the -200 dB "
        "floor, negative bins and padded layout, rect ENBW and `nfft` were "
        "absent. Now C T1-T5 and T18 (#1955). The sharpest illustration is a "
        "sabotage: making the dB reference use `n^2` instead of `cg^2` left "
        "the existing kernel-equality test GREEN, because both of its paths "
        "share the reference; rect cannot see it either, since `cg = n` "
        "there. Only T2, over the tapered windows, goes red.",
    )
    R.find(
        "F2",
        "FIXED",
        "**Three existing checks were vacuous.** Reset re-fed identical "
        "frames in mean mode, where Welford's count = 1 step re-seeds "
        "whatever reset did; `max >= min` per bin passes with two MEAN "
        "states; and the band partition compared `total_band_power` with "
        "the sum of the same per-band values. Each is replaced by an "
        "external truth in C (T9, T10, T12; #1956) and measured again in "
        "§2.4 and §2.5.",
    )
    R.find(
        "F3",
        "FIXED",
        "**The certification's own first tests had to be corrected, and the "
        "corrections are the evidence they measure something.** (i) T8's "
        "fold check fed real frames only; on a Hermitian frame +k and -k "
        "carry equal power, so a fold that doubled +k passed — sabotage S8 "
        "found it, and the complex-input fold (§2.7) closed it. (ii) The "
        "noise-floor level cannot tell a median from a mean of the dB "
        "values: at K = 256 they differ by ~0.003 dB. T14b puts tones on "
        "1/8 of the bins, which moves a median by +0.045 dB and a mean by "
        "+3 dB (§2.6). (iii) The first SFDR layout carried a 5.1e-3 dB "
        "leakage bias, modelled in double to 1e-6 dB before the tones moved "
        "to N/2 apart (§2.6).",
    )
    R.find(
        "F4",
        "C-ONLY",
        "`dp_psd_frame_power`, `dp_psd_frame_db` and `dp_psd_frame_linear` "
        "(#1963), the per-frame kernel the Spectrogram composes, have no "
        "Python binding. They are "
        "certified in `native/tests/test_psd_core.c`: one frame equals "
        "accumulate-then-read bit for bit (4 windows x n {64, 100} x pad "
        "{1, 2}), the average is untouched (T1), 0 dBFS under every window "
        "and both references (T2), the floor (T3) and the layout (T4); "
        "`frame_linear` in its own block: 10·log10 of it is `frame_db` above "
        "the floor, and a full-scale bin tone reads 1.0 under every window, "
        "padded or not, against `full_scale` and `bits`. §2.1-§2.2 reach the "
        "same transform through `accumulate`.",
    )
    R.find(
        "F5",
        "FIXED",
        "**The header described `n` where the code uses `nfft`.** `psd_db` "
        f"writes `nfft` values ({d.a_len_padded} at `n = 64, pad = 2`), "
        "`psd_dbhz_max_out` is `nfft`, and the bin-to-frequency map runs "
        "over `nfft`; the file comment named two windows of four, and "
        "`beta` was 'ignored' for two of the three other windows. Prose, "
        "which jm carried to both faces; corrected in the header (#1959, "
        "§2.10 (a)).",
    )
    R.find(
        "F6",
        "FIXED",
        "**`pad = 0` was silently 1**, though the header documents "
        "`pad (>= 1)`. Refused now, like `n < 2` (#1959, §2.10 (b)).",
    )
    R.find(
        "F7",
        "FIXED",
        "**An exp-mode `alpha` outside `(0, 1]` was accepted and read "
        "wrong:** 0 never leaves the first frame, a negative alpha "
        "extrapolates (1x, 16x, 1x frames read -10.25 at -0.5), and above 1 "
        "is pass-through. AccTrace owns the rule; its create refuses and "
        "PSD returns that NULL (#1959, §2.10 (c)). AccTrace's own runtime "
        "`alpha` setter is outside PSD's face, which has no setter, and its "
        "two follow-ups live there: a refused value is kept silently in "
        "Python until just-makeit#2182 (#1987), and the value is not yet in "
        "AccTrace's state blob (#2000).",
    )
    R.find(
        "F8",
        "FIXED",
        "**`occupied_bw` accepted any fraction, through a private copy of "
        "the search.** A fraction outside the open interval `(0, 1)` reads "
        "NaN, `dp_obw_from_power`'s rule, and PSD's copy is deleted: it "
        "hands the averager's own double trace to the primitive. Without "
        "the copy's `cg^2` division equal bins sum exactly, so an edge on a "
        "bin boundary reads the exact width; C tie pins go red under that "
        "division at -O0, -O2 and -O3 (#1959, §2.10 (c)).",
    )
    R.find(
        "F9",
        "FIXED",
        "**A window with no gain built an estimator that read NaN or the "
        "floor.** Hann at `n = 2` is `[0, 0]`; a Kaiser `beta` of NaN, or "
        "from about 2.3e5 up, makes every tap NaN; a NaN `fs` or "
        "`full_scale` passed `<= 0.0`; `bits` above 64 overflows the "
        "reference. Each is refused at create (#1959, §2.10 (f)). One "
        "caller reached the Hann case mid-stream, AsyncDsssReceiver's "
        "carrier estimator at `refine_n_fft` 1 or 2, so carrier_acq's block "
        "floor is now 3, the shortest whose Hann has gain.",
    )
    R.find(
        "F10",
        "FIXED",
        "**`band_power` returned an empty array where its four siblings "
        "return `None`.** It returns `None` before a frame, or without a "
        "complete lo/hi pair (#1959, §2.10 (g)).",
    )
    R.find(
        "F11",
        "FIXED",
        "**`dp_psd_create(1 << 62, ...)` crashed:** `n * pad` and the byte "
        "counts wrapped. Refused before any allocation now, not left to "
        "calloc, which ASan and TSan report as an error rather than a NULL "
        "(#1959, §2.10 (i)).",
    )
    rect = d.e_rows[0]
    e_excess = float(rect[3]) - float(rect[1])
    R.find(
        "F12",
        "GAP",
        "**A scalar readout's 0.0 means both 'no measurement' and a real "
        "0 dB.** A floor scaled to 0 dB reads "
        f"`noise_floor() = {d.d_zero_floor:+.4f}`, next to the empty "
        f"state's `0.0`, and `snr` over a band outside the span returns "
        f"{d.d_snr_outside!r}, the same as 'no SNR' (§2.10 (d)). Which "
        "sentinel is an API decision: #1957.",
    )
    R.find(
        "F13",
        "GAP",
        "**Adjacent bands both count the bin they share**, so a partition "
        "overstates the whole: a DC tone read as `[-fs/2, 0]` plus "
        f"`[0, fs/2]` sums {e_excess:+.2f} dB above the whole span under "
        "the rectangular window (§2.10 (e)). Which band owns a shared bin "
        "is an API decision: #1958.",
    )
    R.find(
        "F14",
        "GAP",
        "**Every refusal above reaches Python as a `MemoryError` with no "
        "reason.** jm can raise the reason a component means "
        "(`create_error`, gh-482), and neither PSD nor AccTrace declares "
        "it yet: #1986.",
    )
    R.find(
        "F15",
        "GAP",
        "**The stubs do not say these readers can return `None`.** A "
        "`none_on_empty` readout's `.pyi` signature omits `| None` until "
        "just-makeit#2183 ships: #2001.",
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
        d.layout_exact,
        "a bin-centred tone at k lands at nfft/2 + k*nfft/n, k in {-7, 0, 5}, "
        "every window, pad {1, 2, 4}; nfft = next_pow_two(n*pad)",
    )
    R.limit(
        d.level_worst_db < 1e-4,
        f"...and reads 0 dBFS whatever the window (worst "
        f"{d.level_worst_db:.1e} dB)",
    )
    R.limit(
        d.bits_is_full_scale and abs(d.fs_tone_db) < 1e-4,
        "an amplitude-FS tone reads 0 dBFS against full_scale = FS; bits = B "
        "is full_scale = 2^(B-1), bit for bit",
    )
    R.limit(
        d.fft_within,
        "every bin of one frame matches numpy's FFT within 2 c log2(nfft) u "
        "of the total power, every window, n {64, 100}, pad {1, 2}",
    )
    R.limit(d.rect_enbw_one, "rect ENBW is exactly 1.0")
    R.limit(
        d.enbw_worst < 1e-6,
        f"ENBW matches the window's definition, Blackman-Harris its "
        f"published coefficients (worst {d.enbw_worst:.1e})",
    )
    R.limit(d.rbw_is_enbw, "rbw = enbw * fs / n")
    R.limit(
        d.mode_within and d.modes_differ,
        "mean, exp, maxhold and minhold each match their defining rule on "
        "numpy's per-frame powers, over frames where all four differ",
    )
    R.limit(
        d.reset_reseeds,
        "reset re-seeds: after a loud frame and a reset, a quiet frame reads "
        "exactly as a fresh state (maxhold, exp)",
    )
    R.limit(
        d.stat_worst_z < 5.0,
        f"dB/Hz reads var/fs and whole-span band power reads var, every "
        f"window, pad {{1, 2}}, within 5 sd (worst |z| {d.stat_worst_z:.2f})",
    )
    R.limit(
        d.dbhz_offset_worst < 1e-4,
        "psd_dbhz - psd_db = 10 log10(cg^2/(fs s2)) in every bin",
    )
    R.limit(
        d.obw_tone_one_bin and 1012 <= d.obw_noise_bins <= 1016,
        f"OBW: one bin for a bin-centred tone; 0.99 of flat noise on the bin "
        f"grid ({d.obw_noise_bins:.0f} of [1012, 1016] bins)",
    )
    R.limit(
        abs(d.nf_dev) < d.nf_tol and abs(d.nf_median_dev) < d.nf_median_tol,
        "the noise floor is the median of the dB spectrum, within 5 sd — "
        "with and without tones on 1/8 of the bins",
    )
    R.limit(abs(d.snr_dev) < d.snr_tol, "SNR of tone plus noise, within 5 sd")
    R.limit(
        abs(d.sfdr - 20.0) < 2e-3 and d.sfdr_one_tone == 0.0,
        "SFDR is carrier minus strongest spur (20 dB to 2e-3), and 0 with "
        "one peak",
    )
    R.limit(
        d.herm_worst <= d.herm_bound and d.fold_exact and d.fold_complex_ok,
        "real input is Hermitian; the one-sided fold keeps DC and Nyquist "
        "and sums +k and -k, on real and complex input",
    )
    R.limit(
        d.empty_ok,
        "before any frame every reader returns None or 0 and total band "
        "power the -200 dB floor; an all-zero frame reads -200 dB",
    )
    R.limit(
        d.linear_ignores_fs,
        "the linear readouts do not apply full_scale; the dB ones do",
    )
    R.limit(
        d.b_pad0_refused
        and d.c_alpha_refused
        and not d.f_created
        and d.i_overflow_refused,
        "create refuses what would read wrong: pad 0, an exp alpha outside "
        "(0, 1], a window with no gain, n * pad past 2^60",
    )
    R.limit(
        d.c_fraction_nan,
        "occupied_bw is NaN for a fraction outside the open interval (0, 1)",
    )
    R.limit(
        d.g_band_empty is None,
        "band_power before any frame is None, like its siblings",
    )
    R.limit(d.state_exact, "a state blob resumes bit-exactly")
    R.limit(d.state_rejects, "a clobbered state envelope is refused")


# ── build ─────────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# PSD — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "PSD",
        [
            "**A row is that frame's PSD.** The per-frame kernel equals "
            "accumulate-then-read bit for bit and reads a full-scale tone at "
            "0 dBFS under every window and both references — the claim the "
            "Spectrogram stands on, certified in C (F4, §2.1).",
            "**The levels are absolute.** dB/Hz reads `var/fs` and band power "
            "reads `var` for every window and pad, within 5 sd of the "
            "estimator's own spread (§2.5).",
            "**The noise floor is a median.** It ignores tones on an eighth "
            "of the bins to within 0.05 dB, where a mean of dB would move "
            "3 dB (§2.6).",
            "**Three of the old tests could not fail.** Reset, max >= min "
            "and the band partition passed whatever the code did; each now "
            "has an external truth (F2).",
            "**Two readouts can mislead, and are open.** A scalar readout's "
            "0.0 is both 'no measurement' and a real 0 dB (#1957), and "
            "adjacent bands both count the bin they share, +3 dB on a DC "
            "tone (#1958). Check for a frame first, and do not sum a "
            "partition (F12, F13).",
            "**create refuses what would read wrong** (pad 0, an alpha "
            "outside (0, 1], a gainless window, an overflowing size), but "
            "Python sees a bare `MemoryError` until #1986 (F6-F11, F14).",
        ],
    )
    R.summary("\n- Raw sweeps: `data/fft_vs_numpy.csv`, `data/density.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
