"""Synth — certification evidence, measured through the binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/wfm/tests/validation/wfm_synth/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/wfm_synth/wfm_synth_core.h` is the SSOT and
`native/tests/test_wfm_synth_core.c` certifies it in C. This file
measures the same properties through `doppler.wfm._SynthEngine` — the
raw engine the `Synth`/`Segment`/`Composer` ladder is built on — to show
the binding delivers them.

**Why the truths here are numpy's and not doppler's.** The object under
test generates the signals every other doppler object is measured with,
so measuring it with doppler would be the consistency trap
`docs/dev/contributing/validation.md` warns about one level up: a shared
defect between the generator and the analyzer cancels, and both look
right. So a tone's frequency is read off an unwrapped numpy phase, a
PN's quality from its own periodic autocorrelation (an m-sequence's is
exactly -1/L at every non-zero lag, which is arithmetic and not an
opinion), and a shaped spectrum from numpy's FFT. None of it owes
anything to doppler.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import _SynthEngine, rrc_taps

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

FS = 1.0e6
SEED = 5

# Every waveform type the enum carries, in enum order. Kept as a list so a
# tenth type cannot be added without a deliberate answer in each sweep --
# the same reason the C test asserts wfm_synth_bps over the whole enum.
TYPES = [
    "tone",
    "noise",
    "pn",
    "bpsk",
    "qpsk",
    "chirp",
    "bits",
    "symbols",
    "dsss",
]

# A short pattern and a short symbol stream, attached to the two types that
# need one. Deliberately not a power of two long, so a cycling bug that
# happened to align with the block size would still show.
BIT_PATTERN = np.array([1, 0, 1, 1, 0, 0], np.uint8)
SYMBOL_STREAM = np.array([1 + 0j, 1j, -1 + 0j, 0.5 - 0.5j], np.complex64)
DSSS_CODE = np.array([1, 0, 0, 1, 1, 0, 1], np.uint8)

# The SNR sweep §2.4 runs. 9 dB is the compose test's operating point, and
# the rest spread far enough that a missing 10log10(span) term (the whole
# failure mode) cannot hide inside the Monte-Carlo error.
SNR_SWEEP = [0.0, 3.0, 6.0, 9.0, 12.0, 15.0]
SNR_N = 400_000

# Roll-offs for the pulse-shaping sweep, and the sps values that take each
# of set_rrc's two branches: 2/4/8 are powers of two (the polyphase
# shaper), 3/5 are not (the dense FIR fallback).
BETAS = [0.2, 0.35, 0.5, 0.9]
SPS_POW2 = [2, 4, 8]
SPS_DENSE = [3, 5]


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def synth(wtype: str, **kw) -> _SynthEngine:
    """One engine, with the attachments its type needs.

    Parameters
    ----------
    wtype : str
        A member of `TYPES`.
    **kw
        Overrides for the create arguments.

    Returns
    -------
    _SynthEngine
        Ready to `step()`/`steps()`.
    """
    args = {
        "fs": FS,
        "freq": 0.0,
        "snr": 100.0,
        "sps": 4,
        "pn_length": 9,
        "seed": SEED,
        "f_end": 0.0,
    }
    args.update(kw)
    if wtype == "chirp" and args["f_end"] == 0.0:
        args["f_end"] = 2.0e5
    s = _SynthEngine(type=wtype, **args)
    if wtype == "bits":
        s.set_bits(BIT_PATTERN, 1)
    elif wtype == "symbols":
        s.set_symbols(SYMBOL_STREAM)
    elif wtype == "dsss":
        s.set_dsss(
            DSSS_CODE,
            3,
            DSSS_CODE,
            np.array([1, 0], np.uint8),
            np.array([1, 1, 0, 1, 0], np.uint8),
            1,
        )
    return s


def ifreq(y: np.ndarray, fs: float = FS) -> np.ndarray:
    """Instantaneous frequency in Hz, from the unwrapped phase.

    The external truth §2.6 scores the chirp against: one numpy unwrap and
    one difference, owing nothing to doppler.
    """
    ph = np.unwrap(np.angle(np.asarray(y).astype(np.complex128)))
    return np.diff(ph) / (2.0 * np.pi) * fs


def power(y: np.ndarray) -> float:
    """Mean power, accumulated in double.

    In float32 this is not a detail: a 400k-sample mean of |y|^2 over a
    unit-power signal loses enough precision to move the measured noise
    power by 0.5%, which is ten times the Monte-Carlo error and would have
    been read as a real offset in §2.4.
    """
    z = np.asarray(y).astype(np.complex128)
    return float(np.mean(np.abs(z) ** 2))


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    type_rows: list[list[str]] = field(default_factory=list)
    pn_rows: list[list[float]] = field(default_factory=list)
    pn_ideal_exact: bool = False
    qpsk_points: int = 0
    qpsk_unit: float = 0.0
    gray_adjacent: bool = False
    gray_rows: list[list[str]] = field(default_factory=list)
    snr_rows: list[list[float]] = field(default_factory=list)
    snr_worst_rel: float = 0.0
    clean_bytes: int = 0
    noisy_bytes: int = 0
    clean_exact: bool = False
    rrc_power_rows: list[list[float]] = field(default_factory=list)
    rrc_power_worst: float = 0.0
    rrc_oob_rows: list[list[float]] = field(default_factory=list)
    rrc_oob_worst: float = 0.0
    rect_oob: float = 0.0
    chirp_rows: list[list[float]] = field(default_factory=list)
    chirp_worst_dev: float = 0.0
    chirp_envelope: float = 0.0
    chirp_step_swept: float = 0.0
    chirp_steps_swept: float = 0.0
    chirp_step_gap: float = 0.0
    chirp_block_gap: float = 0.0
    face_rows: list[list[str]] = field(default_factory=list)
    face_identical: list[str] = field(default_factory=list)
    face_differs: list[str] = field(default_factory=list)
    chunk_identical: list[str] = field(default_factory=list)
    chunk_differs: list[str] = field(default_factory=list)
    reset_all: bool = False
    state_rows: list[list[str]] = field(default_factory=list)
    state_all_exact: bool = False
    cycle_bits: bool = False
    cycle_symbols: bool = False
    unreachable: list[str] = field(default_factory=list)
    accessors_ok: bool = False


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`Synth` is the bottom of the wfmgen ladder: one streaming source "
        "that combines a symbol generator, a carrier and additive noise, "
        "and that every `Segment`, `Timeline` and `Composer` above it is "
        "built out of. The design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md); "
        "the API is "
        "`native/inc/wfm_synth/wfm_synth_core.h`, certified in C by "
        "`native/tests/test_wfm_synth_core.c`. Neither is restated here."
    )
    R.md()
    R.md(
        "Measured through `doppler.wfm._SynthEngine`, the raw engine. The "
        "public `Synth` is a thin declarative wrapper over the same core, "
        "so a property proven here holds for the ladder above it -- with "
        "one exception, F2, where the wrapper's face is the one that "
        "matters and is measured directly."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_types(d: Data) -> None:
    R.md("### 2.1 The nine waveform types (C §create)")
    R.md()
    R.md(
        "What each type emits, and the truth it is scored against. Nothing "
        "here is a verdict -- the numbers are in the sections that follow."
    )
    R.md()
    for t in TYPES:
        y = synth(t).steps(4096)
        z = y.astype(np.complex128)
        finite = bool(np.all(np.isfinite(z)))
        d.type_rows.append(
            [
                t,
                f"{power(y):.4f}",
                f"{np.max(np.abs(z)):.4f}",
                "yes" if finite else "**no**",
            ]
        )
    R.table(["type", "mean power", "peak |y|", "all finite"], d.type_rows)
    R.md(
        "Every type produces finite samples at a sane level. `noise` sits "
        "at unit power by construction (the amplitude is 1/sqrt(2) per "
        "component); `tone` is exactly 1; the modulated types are 1 "
        "because their constellations are unit-modulus, which is what "
        "makes the SNR reference in §2.4 mean what it says."
    )
    R.md()


def measure_pn(d: Data) -> None:
    R.md("### 2.2 The PN is a maximal-length sequence (C §create)")
    R.md()
    R.md(
        "The header says `pn_length` gives period 2^n - 1 and that poly 0 "
        "looks up the canonical MLS polynomial. The check is the defining "
        "property of an m-sequence rather than a comparison against "
        "another doppler component: its periodic autocorrelation is "
        "exactly `-1/L` at EVERY non-zero lag. A sequence that is merely "
        "pseudo-random would not be, and a wrong tap polynomial gives a "
        "shorter period whose autocorrelation shows a second peak."
    )
    R.md()
    rows = []
    exact = True
    for n in (7, 9, 11):
        L = 2**n - 1
        c = np.real(
            synth("pn", sps=1, pn_length=n).steps(L).astype(np.complex128)
        )
        ac = np.array([float(np.dot(c, np.roll(c, k)) / L) for k in range(L)])
        off = ac[1:]
        ideal = -1.0 / L
        ok = bool(np.allclose(off, ideal, atol=1e-9))
        exact = exact and ok
        rows.append([n, L, ac[0], off.max(), off.min(), ideal])
        d.pn_rows.append([float(n), float(L), float(off.max()), ideal])
    d.pn_ideal_exact = exact
    R.table(
        ["n", "L = 2^n-1", "peak", "off-peak max", "off-peak min", "ideal"],
        [[str(r[0]), str(r[1])] + [f"{v:+.6f}" for v in r[2:]] for r in rows],
    )
    R.md(
        "Every off-peak lag equals `-1/L` to 1e-9, at all three register "
        "lengths -- the sequence is maximal-length, not approximately so."
    )
    R.md()
    _csv(
        DATA / "pn_autocorr.csv",
        "n,L,off_peak_max,ideal",
        d.pn_rows,
    )


def measure_constellation(d: Data) -> None:
    R.md("### 2.3 The constellation, and the Gray label (C §bits map)")
    R.md()
    y = synth("qpsk", sps=1).steps(4000).astype(np.complex128)
    pts = np.unique(np.round(y, 6))
    d.qpsk_points = len(pts)
    d.qpsk_unit = float(np.max(np.abs(np.abs(pts) - 1.0)))
    R.md(
        f"QPSK emits exactly {d.qpsk_points} distinct points, every one of "
        f"unit modulus to {d.qpsk_unit:.2e} -- so the Es/No reference in "
        "§2.4 is referred to a genuinely unit-power symbol."
    )
    R.md()
    R.md(
        "The header is emphatic that one bits->symbol map serves every "
        "order, because a private QPSK copy once put b0 on the I sign and "
        "b1 on the Q sign: the same four points, two labels exchanged, "
        "which scores about half the bits wrong on a working receiver and "
        "produces no visibly odd waveform. `mpsk_constellation` is not "
        "bound in Python, so the identity is certified in C. What Python "
        "can check is the property that makes the map worth having, and it "
        "needs no reference implementation: **adjacent labels differ in "
        "exactly one bit.**"
    )
    R.md()
    pat = np.array([0, 0, 0, 1, 1, 0, 1, 1], np.uint8)
    s = _SynthEngine(type="bits", fs=FS, freq=0.0, snr=100.0, sps=1, seed=1)
    s.set_bits(pat, 2)
    sym = s.steps(4).astype(np.complex128)
    order = [0, 1, 3, 2]  # 00, 01, 11, 10 -- Gray order around the circle
    ang = [float(np.angle(sym[i])) for i in order]
    steps = np.diff(np.unwrap([*ang, ang[0] + 2 * np.pi]))
    d.gray_adjacent = bool(np.allclose(np.abs(steps), np.pi / 2, atol=1e-5))
    for g, z in zip((0, 1, 2, 3), sym):
        d.gray_rows.append([f"{g:02b}", f"{z.real:+.4f}", f"{z.imag:+.4f}"])
    R.table(["bits (MSB first)", "I", "Q"], d.gray_rows)
    R.md(
        "Walking the labels in Gray order -- 00, 01, 11, 10 -- steps a "
        "quarter turn each time"
        + (
            ", so a symbol error between neighbours costs one bit, not two."
            if d.gray_adjacent
            else " **-- it does not, and that is a defect.**"
        )
    )
    R.md()


def measure_snr(d: Data) -> None:
    R.md("### 2.4 The SNR contract (C §A, §B)")
    R.md()
    R.md(
        "The one piece of arithmetic in this object whose failure is "
        "silent: get it wrong and the waveform is still a waveform, at an "
        "SNR nobody asked for. Three references -- `fs`, `esno`, `ebno` -- "
        "and the conversion to noise power over the full rate is "
        "`snr_fs = snr [+ 10log10(bps)] - 10log10(span)`."
    )
    R.md()
    R.md(
        "Scored against that formula evaluated in numpy, never against the "
        "library's own converter. The signal is unit power in every case, "
        "so the expected total is `1 + 10^(-snr_fs/10)` and the residual "
        "is the noise placement alone."
    )
    R.md()
    rows = []
    worst = 0.0
    cases = [
        ("tone", "fs", 8, 1),
        ("bpsk", "fs", 8, 1),
        ("bpsk", "esno", 8, 1),
        ("bpsk", "ebno", 8, 1),
        ("qpsk", "esno", 4, 2),
        ("qpsk", "ebno", 4, 2),
        ("qpsk", "esno", 8, 2),
    ]
    for t, mode, sps, bps in cases:
        for snr in SNR_SWEEP:
            y = synth(t, snr=snr, snr_mode=mode, sps=sps, pn_length=11).steps(
                SNR_N
            )
            got = power(y)
            fsdb = snr - 10 * np.log10(sps)
            if mode == "ebno":
                fsdb += 10 * np.log10(bps)
            elif mode == "fs":
                fsdb = snr
            want = 1.0 + 10 ** (-fsdb / 10.0)
            rel = abs(got - want) / want
            worst = max(worst, rel)
            rows.append([t, mode, sps, snr, got, want, rel])
            d.snr_rows.append([float(sps), snr, got, want, rel])
    d.snr_worst_rel = float(worst)
    R.table(
        ["type", "mode", "sps", "snr dB", "measured P", "expected P", "rel"],
        [
            [
                r[0],
                r[1],
                str(r[2]),
                f"{r[3]:.1f}",
                f"{r[4]:.4f}",
                f"{r[5]:.4f}",
                f"{r[6] * 100:.3f}%",
            ]
            for r in rows
        ],
    )
    R.md(
        f"Worst deviation across all {len(rows)} cases is "
        f"{worst * 100:.2f}%, which is the Monte-Carlo error of a "
        f"{SNR_N}-sample power estimate and not an offset: the standard "
        "error of a noise-power mean is P/sqrt(N), about 0.16% here."
    )
    R.md()
    _csv(
        DATA / "snr_contract.csv",
        "sps,snr_db,measured_power,expected_power,rel_error",
        d.snr_rows,
    )

    R.md("#### The 100 dB clean cutoff")
    R.md()
    R.md(
        "`snr >= 100` skips AWGN entirely, so a clean waveform pays no "
        "noise cost. That boundary is invisible in the samples -- at 99.9 "
        "dB the noise is 1e-10 of the signal, far below float32 resolution "
        "-- so it is measured where it is actually observable: the "
        "serialized state, whose children are presence-flagged."
    )
    R.md()
    d.clean_bytes = int(synth("tone", snr=100.0, sps=8).state_bytes())
    d.noisy_bytes = int(synth("tone", snr=99.9, sps=8).state_bytes())
    clean = synth("tone", snr=100.0, sps=8).steps(4096).astype(np.complex128)
    d.clean_exact = bool(np.all(clean == 1.0 + 0.0j))
    R.table(
        ["snr", "state bytes", "AWGN child"],
        [
            ["100.0 (clean)", str(d.clean_bytes), "absent"],
            ["99.9", str(d.noisy_bytes), "present"],
        ],
    )
    R.md(
        f"The cutoff is a structural switch, not a numerical one: "
        f"{d.noisy_bytes - d.clean_bytes} bytes of AWGN state appear the "
        "moment the requested SNR drops below 100 dB, while the samples on "
        "either side of the boundary are indistinguishable in float32. A "
        "clean tone is exactly `1+0j` at every sample."
    )
    R.md()
    R.md("![Realized noise power against each reference](snr.png)")
    R.md()


def measure_shaping(d: Data) -> None:
    R.md("### 2.5 Pulse shaping: unit power, and the occupied band (C §G)")
    R.md()
    R.md(
        "`set_rrc` scales the caller's taps by sqrt(sps) internally so the "
        "symbol-rate impulse train -- one impulse every sps samples, mean "
        "power 1/sps -- comes back out at unit power. That scaling is what "
        "makes the SNR reference in §2.4 survive shaping, so it is "
        "measured across both of set_rrc's branches: a power-of-two sps "
        "takes the polyphase shaper, anything else the dense FIR."
    )
    R.md()
    rows = []
    worst = 0.0
    for sps in SPS_POW2 + SPS_DENSE:
        branch = "polyphase" if (sps & (sps - 1)) == 0 else "dense FIR"
        for beta in BETAS:
            s = synth("bpsk", sps=sps, pn_length=11)
            s.set_rrc(np.asarray(rrc_taps(beta, sps, 12), np.float32))
            p = power(s.steps(200_000))
            worst = max(worst, abs(p - 1.0))
            rows.append([sps, branch, beta, p])
            d.rrc_power_rows.append([float(sps), beta, p])
    d.rrc_power_worst = float(worst)
    R.table(
        ["sps", "branch", "beta", "mean power"],
        [[str(r[0]), r[1], str(r[2]), f"{r[3]:.4f}"] for r in rows],
    )
    R.md(
        f"Unit power to within {worst * 100:.2f}% everywhere, and the two "
        "branches agree with each other -- the caller passes raw "
        "`rrc_taps()` output and does not scale anything."
    )
    R.md()

    R.md("#### The band the shaping buys")
    R.md()
    R.md(
        "An RRC with roll-off beta occupies `(1+beta) * Rs`. Measured as "
        "the fraction of power outside that edge, from numpy's FFT, with "
        "the unshaped rectangular hold as the control -- without one, a "
        "containment number proves nothing, because any signal contains "
        "most of its power somewhere."
    )
    R.md()
    fs, sps = FS, 8
    rs = fs / sps
    rows = []
    worst_oob = 0.0
    for beta in BETAS:
        s = synth("bpsk", sps=sps, pn_length=11)
        s.set_rrc(np.asarray(rrc_taps(beta, sps, 12), np.float32))
        oob = _oob(s.steps(1 << 16), (1 + beta) * rs / 2, fs)
        worst_oob = max(worst_oob, oob)
        rows.append([beta, (1 + beta) * rs / 2 / 1e3, oob * 100])
        d.rrc_oob_rows.append([beta, oob])
    d.rrc_oob_worst = float(worst_oob)
    d.rect_oob = float(
        _oob(
            synth("bpsk", sps=sps, pn_length=11).steps(1 << 16),
            (1 + 0.35) * rs / 2,
            fs,
        )
    )
    R.table(
        ["beta", "edge (1+b)Rs/2 kHz", "power beyond the edge"],
        [[str(r[0]), f"{r[1]:.2f}", f"{r[2]:.4f}%"] for r in rows]
        + [
            [
                "rect (control)",
                f"{(1 + 0.35) * rs / 2 / 1e3:.2f}",
                f"{d.rect_oob * 100:.4f}%",
            ]
        ],
    )
    R.md(
        f"Shaping confines all but {worst_oob * 100:.4f}% of the power "
        f"inside `(1+beta)*Rs`. The same BPSK stream unshaped leaks "
        f"{d.rect_oob * 100:.1f}% past the same edge -- a rectangular hold "
        "is a sinc in frequency and never stops. That ratio, about "
        f"{d.rect_oob / max(worst_oob, 1e-12):.0f}x, is what the filter is "
        "for."
    )
    R.md()
    R.md("![Shaped spectra against the rectangular control](spectrum.png)")
    R.md()
    _csv(
        DATA / "rrc_containment.csv",
        "beta,out_of_band_fraction",
        d.rrc_oob_rows,
    )


def _oob(y: np.ndarray, edge: float, fs: float) -> float:
    """Fraction of power beyond +/- `edge`, from numpy's FFT."""
    z = np.asarray(y).astype(np.complex128)
    n = len(z)
    p = np.abs(np.fft.fftshift(np.fft.fft(z * np.hanning(n)))) ** 2
    f = np.fft.fftshift(np.fft.fftfreq(n, 1.0 / fs))
    return float(p[np.abs(f) > edge].sum() / p.sum())


def measure_chirp(d: Data) -> None:
    R.md("### 2.6 The chirp sweeps linearly -- when its span is pinned")
    R.md()
    R.md(
        "A linear FM sweep's slope is `(f_end - f_start)/span`, so the "
        "span has to be known before generation. Scored against a "
        "straight-line fit to the instantaneous frequency, which is one "
        "numpy unwrap and one difference."
    )
    R.md()
    rows = []
    worst_dev = 0.0
    env = 0.0
    n = 4096
    for f0, f1 in [(0.0, 2.0e5), (2.0e5, -1.0e5), (-1.5e5, 1.5e5)]:
        y = synth("chirp", freq=f0, f_end=f1).steps(n).astype(np.complex128)
        inst = ifreq(y)
        x = np.arange(len(inst))
        slope, icept = np.polyfit(x, inst, 1)
        dev = float(np.max(np.abs(inst - (icept + slope * x))))
        worst_dev = max(worst_dev, dev)
        env = max(env, float(np.std(np.abs(y))))
        rows.append([f0 / 1e3, f1 / 1e3, slope, (f1 - f0) / n, dev])
        d.chirp_rows.append([f0, f1, float(slope), (f1 - f0) / n, dev])
    d.chirp_worst_dev = worst_dev
    d.chirp_envelope = env
    R.table(
        ["f_start kHz", "f_end kHz", "slope Hz/sa", "ideal", "max dev Hz"],
        [
            [
                f"{r[0]:+.1f}",
                f"{r[1]:+.1f}",
                f"{r[2]:.4f}",
                f"{r[3]:.4f}",
                f"{r[4]:.2f}",
            ]
            for r in rows
        ],
    )
    R.md(
        f"The slope matches the ideal to four decimals in all three cases, "
        f"up-chirp and down, with at most {worst_dev:.2f} Hz of deviation "
        f"from the straight line across a 200 kHz sweep and an envelope "
        f"standard deviation of {env:.1e} -- a pure FM tone, constant "
        "modulus, as it should be."
    )
    R.md()

    R.md("#### And when it is not pinned, it is three different waveforms")
    R.md()
    R.md(
        "The span above was pinned by the single `steps(n)` call that read "
        "the whole sweep -- `steps()` self-pins on its first call. Nothing "
        "else does. Read the SAME chirp another way and the result changes "
        "completely (**F2**, gh-1115)."
    )
    R.md()
    kw = {"freq": 0.0, "f_end": 2.0e5}
    one = synth("chirp", **kw).steps(n)
    e = synth("chirp", **kw)
    stepd = np.array([e.step() for _ in range(n)], np.complex64)
    e = synth("chirp", **kw)
    blocks = np.concatenate([e.steps(64) for _ in range(n // 64)])
    sw = lambda y: float(ifreq(y).max() - ifreq(y).min())  # noqa: E731
    d.chirp_steps_swept = sw(one)
    d.chirp_step_swept = sw(stepd)
    d.chirp_step_gap = float(np.max(np.abs(one - stepd)))
    d.chirp_block_gap = float(np.max(np.abs(one - blocks)))
    R.table(
        [
            "how the same chirp was read",
            "frequency swept",
            "max diff vs one call",
        ],
        [
            [
                f"`steps({n})`, one call",
                f"{d.chirp_steps_swept / 1e3:.1f} kHz",
                "--",
            ],
            [
                f"`step()` x {n}",
                f"{d.chirp_step_swept / 1e3:.1f} kHz",
                f"{d.chirp_step_gap:.3f}",
            ],
            [
                f"`steps(64)` x {n // 64}",
                f"{sw(blocks) / 1e3:.1f} kHz",
                f"{d.chirp_block_gap:.3f}",
            ],
        ],
    )
    R.md(
        f"`step()` sweeps {d.chirp_step_swept:.0f} Hz: with the span never "
        "pinned the slope stays 0 and the output is a constant-frequency "
        "tone at `f_start`. The difference from the correct waveform is "
        f"{d.chirp_step_gap:.3f} -- the largest two unit-modulus signals "
        "can differ by."
    )
    R.md()
    R.md("![One chirp, three read patterns](chirp.png)")
    R.md()


def measure_faces(d: Data) -> None:
    R.md("### 2.7 One waveform, two faces (C §steps)")
    R.md()
    R.md(
        "`step()` and `steps()` must be the same waveform, and a block "
        "read must not depend on the block size -- both are what let a "
        "caller swap a streaming loop for a bulk render, and what the "
        "composer relies on when it renders a segment in chunks."
    )
    R.md()
    rows = []
    for t in TYPES:
        a = synth(t).steps(64)
        s = synth(t)
        b = np.array([s.step() for _ in range(64)], np.complex64)
        same_face = bool(np.array_equal(a, b))
        s = synth(t)
        c = np.concatenate([s.steps(16) for _ in range(4)])
        same_chunk = bool(np.array_equal(a, c))
        (d.face_identical if same_face else d.face_differs).append(t)
        (d.chunk_identical if same_chunk else d.chunk_differs).append(t)
        rows.append(
            [
                t,
                "identical" if same_face else "**DIFFERS**",
                "identical" if same_chunk else "**DIFFERS**",
            ]
        )
    d.face_rows = rows
    R.table(["type", "step() vs steps()", "steps(64) vs 4 x steps(16)"], rows)
    R.md(
        f"Bit-for-bit on {len(d.face_identical)} of {len(TYPES)} types, "
        f"both ways. The exception is `chirp`, for the reason §2.6 "
        "measured: the span is inferred from how the caller read, so the "
        "two faces are answering different questions rather than "
        "disagreeing about one (F2)."
    )
    R.md()


def measure_state(d: Data) -> None:
    R.md("### 2.8 Reset, and the serialized state")
    R.md()
    ok_reset = True
    for t in TYPES:
        s = synth(t, snr=6.0, snr_mode="fs")
        a = s.steps(128).copy()
        s.reset()
        ok_reset = ok_reset and bool(np.array_equal(a, s.steps(128)))
    d.reset_all = ok_reset
    R.md(
        "`reset()` rewinds every child -- LO phase, AWGN stream, PN "
        "register, shaper delay line -- so the sequence repeats from "
        "sample 0"
        + (
            f". True for all {len(TYPES)} types, with noise on."
            if ok_reset
            else " **-- and it does not.**"
        )
    )
    R.md()
    R.md(
        "The state triplet is the elastic-resume face: a mid-stream "
        "hand-off must resume bit-for-bit into a fresh instance. Split at "
        "200 of 600 samples, with noise on so the RNG has to carry too."
    )
    R.md()
    rows = []
    all_exact = True
    for t in TYPES:
        if t == "chirp":
            continue  # its span depends on the read pattern -- see F2
        kw = {"snr": 6.0, "snr_mode": "fs", "freq": 1.0e5}
        ref = synth(t, **kw).steps(600)
        a = synth(t, **kw)
        b = synth(t, **kw)
        part = a.steps(200)
        blob = a.get_state()
        b.steps(200)
        b.set_state(blob)
        cont = b.steps(400)
        exact = bool(np.array_equal(np.concatenate([part, cont]), ref))
        all_exact = all_exact and exact
        rows.append(
            [t, str(len(blob)), "bit-exact" if exact else "**DIVERGED**"]
        )
    d.state_rows = rows
    d.state_all_exact = all_exact
    R.table(["type", "blob bytes", "resume"], rows)
    R.md()
    s = synth("bits", sps=3)
    y = s.steps(36).astype(np.complex128)
    d.cycle_bits = bool(np.array_equal(y[:18], y[18:]))
    s = synth("symbols", sps=2)
    y = s.steps(16).astype(np.complex128)
    d.cycle_symbols = bool(np.array_equal(y[:8], y[8:]))
    R.md(
        "The two user-supplied sources cycle at exactly `n * sps` samples "
        f"-- a 6-bit pattern at sps 3 repeats every 18 "
        f"({'confirmed' if d.cycle_bits else '**it does not**'}), a "
        f"4-symbol stream at sps 2 every 8 "
        f"({'confirmed' if d.cycle_symbols else '**it does not**'})."
    )
    R.md()


def measure_accessors(d: Data) -> None:
    R.md("### 2.9 The accessors, and what Python cannot reach")
    R.md()
    e = synth("qpsk", sps=4)
    ok = e.get_wtype() == 4 and e.get_nsps() == 4
    e.set_nsps(2)
    ok = ok and e.get_nsps() == 2
    e.set_nsps(4)
    e.reset()
    ok = ok and e.get_sym_pos() == 0
    for k in range(1, 10):
        e.step()
        ok = ok and e.get_sym_pos() == k % 4
    e.reset()
    e.step()
    ok = ok and abs(abs(e.get_cur_re()) - 0.70710678) < 1e-6
    e.set_cur_re(0.25)
    ok = ok and abs(e.get_cur_re() - 0.25) < 1e-9
    d.accessors_ok = bool(ok)
    R.md(
        "All ten get/set accessors are bound and behave: `sym_pos` counts "
        "0..nsps-1 and wraps, the held symbol reads back as the QPSK leg "
        "+-1/sqrt(2), and an injected value survives the round trip"
        + (". " if ok else " **-- one of these fails.** ")
        + "They had no C coverage at all before this certification (F3)."
    )
    R.md()
    d.unreachable = [
        (
            "`wfm_synth_set_chirp_span` -- the only way to pin a chirp's "
            "sweep. Its absence is what makes F2 unfixable from Python."
        ),
        (
            "`wfm_synth_set_dsss_chips` -- installs a pre-assembled burst; "
            "the four-field `set_dsss` is bound and routes through it."
        ),
        (
            "`wfm_synth_reseed_noise` -- fresh noise per segment repeat, "
            "used by the composer in C."
        ),
        (
            "`wfm_synth_noise_steps` -- renders a segment's off-time gap, "
            "used by the composer in C."
        ),
        (
            "`wfm_synth_snr_over_fs` / `wfm_synth_bps` -- header inlines; "
            "the conversion they perform is measured end-to-end in §2.4."
        ),
    ]
    R.md(
        "**C-ONLY.** Five entry points the header declares are not on the "
        "Python face. Each is certified in C instead -- the new §C, §D, "
        "§E and §A of `test_wfm_synth_core.c` -- and none is a gap in the "
        "binding except the first:"
    )
    R.md()
    for u in d.unreachable:
        R.md(f"- {u}")
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Each heading names the C section "
        "it tracks where there is one; the numbering is this report's own."
    )
    R.md()
    d = Data()
    measure_types(d)
    measure_pn(d)
    measure_constellation(d)
    measure_snr(d)
    measure_shaping(d)
    measure_chirp(d)
    measure_faces(d)
    measure_state(d)
    measure_accessors(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The arithmetic the header calls 'the one place this lives' was "
        "tested by nothing.** `wfm_synth_snr_over_fs` and "
        "`wfm_synth_bps` had zero mentions in every C test in the tree, "
        "and the header's own comment says the failure mode is silent -- "
        "the waveform is still a waveform, at an SNR nobody asked for. "
        "The only thing pinning either was `test_wfm_compose.c`'s "
        "noise-power table, which reaches them one layer up through "
        "`wfm_snr_over_fs` and only for the seven segment-level cases it "
        "lists; the span guard and the whole `bps` enum were outside it. "
        "Closed by §A of `test_wfm_synth_core.c`, against literals derived "
        "by hand from the doc comment -- never by calling the function, "
        "which is the mistake the compose test already had to unlearn. "
        "Sabotage: dropping the `10log10(bps)` term from the Eb/No branch "
        "goes red; so does removing the non-positive span guard.",
    )
    R.find(
        "F2",
        "CONFIRMED",
        "**`step()` on a chirp emits a flat CW tone, and an unpinned "
        "chirp's sweep depends on how the caller chunked its reads** "
        "(gh-1115). The slope needs a span; `wfm_synth_steps()` self-pins "
        "to its first block length and `wfm_synth_step()` has no such "
        "lock, so `chirp_k` stays 0.0 and the output is a constant tone at "
        f"`f_start` -- {d.chirp_step_swept:.0f} Hz swept against "
        f"{d.chirp_steps_swept / 1e3:.0f} kHz, a difference of "
        f"{d.chirp_step_gap:.3f}, the largest two unit-modulus signals can "
        "differ by. Reading in 64-sample blocks instead gives a third "
        "waveform. This contradicts a documented invariant of the object "
        "-- the chirp branch's own comment says step() and steps() 'stay "
        "byte-identical' -- and the C test never caught it because all "
        "three of its chirp cases pin the span first, including the one "
        "asserting the two faces agree. Python cannot pin at all: "
        "`set_chirp_span` is not bound on either face. Reproduced on the "
        "public `doppler.wfm.chirp()`, not just the raw engine. Left "
        "unfixed here on purpose -- phase 8 measures, it does not repair, "
        "and the right repair is a design choice (§2.6).",
    )
    R.find(
        "F3",
        "FIXED",
        "**Four public entry points and all ten accessors had zero C "
        "coverage.** `wfm_synth_set_dsss_chips`, `wfm_synth_reseed_noise` "
        "and the five get/set pairs were mentioned in no C test anywhere "
        "in the tree; `wfm_synth_noise_steps` was reachable only through "
        "`test_wfm_compose.c`, so the object's own claims about it -- "
        "seamless continuation, exact zeros for a clean synth -- were "
        "certified only as a side effect of testing the composition. "
        "Closed by §C, §D, §E and §F. Each was proven by sabotage: "
        "`set_dsss_chips` borrowing instead of copying, `reseed_noise` "
        "doing nothing, `set_sym_pos` doing nothing -- all now red.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "**`create()` and the composer resolve `auto` differently for "
        "dsss, and they must.** `wfm_synth_create` resolves an `auto` SNR "
        "mode to fs for a DSSS source while `wfm_snr_over_fs` resolves the "
        "same source to Es/No. That reads as drift and is not: the codes "
        "that set the spreading factor attach AFTER create(), so the "
        "generator cannot refer a data-symbol Es/N0 to fs and the composer "
        "can. Both files document it; nothing asserted it, so the two "
        "silently agreeing would have moved every bundled DSSS source's "
        "noise by 10log10(sf*sps) with no test objecting. Now pinned by "
        "§B, which measures a codeless dsss synth's noise power (its "
        "output IS the noise term) against the fs answer and against the "
        "Es/No one it must not give.",
    )
    R.find(
        "F5",
        "GAP",
        "**`noise_steps` matches `steps()`'s AWGN chunking to defend "
        "against a code path that no longer exists, and nothing would "
        "catch its return** (gh-1114). The chunk size is justified in "
        "comment by the vectorized AWGN generator not being "
        "block-boundary invariant -- but that generator was removed in "
        "gh-690, leaving a scalar per-sample loop that is invariant by "
        "construction. Measured: sabotaging `CH` from 2048 to 1024 and "
        "rebuilding leaves §E green, on a 3000-sample gap chosen to cross "
        "the boundary. It was the only sabotage in the pass that did not "
        "go red. The chunking is correct and should stay -- `awgn_core.c` "
        "argues the vectorised path is worth restoring -- but if it "
        "returns, this property becomes load-bearing and is ungated, and "
        "gh-690's own post-mortem records that the last time these two "
        "paths disagreed 'nothing noticed'.",
    )
    R.find(
        "F6",
        "C-ONLY",
        "**The bits->symbol map's identity with the library's is certified "
        "in C.** `mpsk_constellation` is not bound in Python, so the claim "
        "that Synth's mapping IS the library's -- the one "
        "`dp_ber_score()` inverts -- cannot be asked from here. It matters "
        "because the failure is invisible: a private QPSK copy put b0 on "
        "the I sign and b1 on the Q, which is the same four points with "
        "two labels exchanged and scores about half the bits wrong on a "
        "working receiver. Certified by the 'bits->symbol map is the "
        "LIBRARY's, at every order' section of `test_wfm_synth_core.c`. "
        "§2.3 measures the property that survives the language boundary: "
        "adjacent labels differ in one bit.",
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
        all(r[3] == "yes" for r in d.type_rows),
        "every one of the nine waveform types produces finite samples",
    )
    R.limit(
        d.pn_ideal_exact,
        "the PN is maximal-length: its periodic autocorrelation is -1/L at "
        "every non-zero lag, to 1e-9, at register lengths 7, 9 and 11",
    )
    R.limit(
        d.qpsk_points == 4 and d.qpsk_unit < 1e-6,
        "QPSK emits exactly four points, all of unit modulus",
    )
    R.limit(
        d.gray_adjacent,
        "the bit->symbol map is Gray: adjacent labels are a quarter turn "
        "apart, so a neighbour error costs one bit",
    )
    R.limit(
        d.snr_worst_rel < 0.01,
        f"realized noise power matches snr [+10log10(bps)] -10log10(span) "
        f"to {d.snr_worst_rel * 100:.2f}% across all three references, "
        "every sps and a 15 dB sweep",
    )
    R.limit(
        d.clean_exact and d.noisy_bytes > d.clean_bytes,
        "snr >= 100 dB builds no AWGN child at all and a clean tone is "
        "exactly 1+0j; one tenth of a dB below, the child is there",
    )
    R.limit(
        d.rrc_power_worst < 0.01,
        f"RRC shaping delivers unit transmit power to "
        f"{d.rrc_power_worst * 100:.2f}%, on BOTH branches -- the caller "
        "passes raw rrc_taps() and scales nothing",
    )
    R.limit(
        d.rrc_oob_worst < 0.001,
        f"shaping confines all but {d.rrc_oob_worst * 100:.4f}% of the "
        "power inside (1+beta)*Rs",
    )
    R.limit(
        d.rect_oob > 0.05,
        f"the unshaped rectangular hold leaks {d.rect_oob * 100:.1f}% past "
        "the same edge -- the containment above is a property of the "
        "filter, not of any signal (the control)",
    )
    R.limit(
        d.chirp_worst_dev < 1.0,
        f"a pinned chirp's instantaneous frequency is linear to "
        f"{d.chirp_worst_dev:.2f} Hz across a 200 kHz sweep, up and down",
    )
    R.limit(
        d.chirp_envelope < 1e-6,
        "and it holds a constant envelope -- a pure FM tone",
    )
    R.limit(
        d.chirp_step_gap > 1.0,
        "an UNPINNED chirp read by step() is a different waveform from the "
        "same chirp read by steps(): recorded so F2 cannot regress "
        "silently into looking fixed",
    )
    R.limit(
        set(d.face_differs) == {"chirp"},
        "step() and steps() are bit-for-bit identical on every type except "
        "chirp, whose span depends on the read (F2)",
    )
    R.limit(
        set(d.chunk_differs) == {"chirp"},
        "and a block read is independent of the block size on those same "
        "types",
    )
    R.limit(
        d.reset_all,
        "reset() rewinds every child, so all nine types repeat from sample "
        "0 with noise on",
    )
    R.limit(
        d.state_all_exact,
        "a mid-stream state hand-off resumes bit-for-bit in a fresh "
        "instance, for every type that has one",
    )
    R.limit(
        d.cycle_bits and d.cycle_symbols,
        "a user bit pattern and a user symbol stream each cycle at exactly "
        "n * sps samples",
    )
    R.limit(
        d.accessors_ok,
        "all ten accessors are bound and behave: sym_pos counts and wraps, "
        "the held symbol reads back, an injected value survives",
    )
    R.limit(
        len(d.unreachable) == 5,
        "five header entry points are not on the Python face and are "
        "certified in C instead -- counted, so one quietly appearing or "
        "vanishing is a change",
    )


# ── plots ────────────────────────────────────────────────────────────
def plots(d: Data) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # 1. the SNR contract
    fig, ax = plt.subplots(figsize=(7, 4))
    for sps, mode, bps, style in [
        (8, "fs", 1, "o-"),
        (8, "esno", 1, "s-"),
        (4, "ebno", 2, "^-"),
    ]:
        got, want = [], []
        for snr in SNR_SWEEP:
            t = "qpsk" if bps == 2 else "bpsk"
            y = synth(t, snr=snr, snr_mode=mode, sps=sps, pn_length=11).steps(
                SNR_N
            )
            got.append(power(y) - 1.0)
            fsdb = snr if mode == "fs" else snr - 10 * np.log10(sps)
            if mode == "ebno":
                fsdb += 10 * np.log10(bps)
            want.append(10 ** (-fsdb / 10.0))
        ax.plot(
            SNR_SWEEP,
            10 * np.log10(got),
            style,
            label=f"{mode}, sps={sps} (measured)",
        )
        ax.plot(SNR_SWEEP, 10 * np.log10(want), "k:", lw=1)
    ax.set_xlabel("requested SNR (dB)")
    ax.set_ylabel("realized noise power (dB)")
    ax.set_title("Every SNR reference lands where its formula says")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "snr.png", dpi=110)
    plt.close(fig)

    # 2. the shaped spectrum, with the rect control
    fig, ax = plt.subplots(figsize=(7, 4))
    sps = 8
    rs = FS / sps
    n = 1 << 16
    f = np.fft.fftshift(np.fft.fftfreq(n, 1.0 / FS)) / 1e3
    for beta in BETAS:
        s = synth("bpsk", sps=sps, pn_length=11)
        s.set_rrc(np.asarray(rrc_taps(beta, sps, 12), np.float32))
        z = s.steps(n).astype(np.complex128)
        p = np.abs(np.fft.fftshift(np.fft.fft(z * np.hanning(n)))) ** 2
        ax.plot(f, 10 * np.log10(p / p.max()), lw=1, label=f"beta={beta}")
        ax.axvline((1 + beta) * rs / 2 / 1e3, color="k", ls=":", lw=0.6)
    z = synth("bpsk", sps=sps, pn_length=11).steps(n).astype(np.complex128)
    p = np.abs(np.fft.fftshift(np.fft.fft(z * np.hanning(n)))) ** 2
    ax.plot(f, 10 * np.log10(p / p.max()), "k--", lw=1, label="rect (control)")
    ax.set_xlim(-250, 250)
    ax.set_ylim(-100, 5)
    ax.set_xlabel("frequency (kHz)")
    ax.set_ylabel("PSD (dB, normalised)")
    ax.set_title("RRC confines the band; a rectangular hold does not")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "spectrum.png", dpi=110)
    plt.close(fig)

    # 3. the chirp, read three ways
    fig, ax = plt.subplots(figsize=(7, 4))
    n = 4096
    kw = {"freq": 0.0, "f_end": 2.0e5}
    one = synth("chirp", **kw).steps(n)
    e = synth("chirp", **kw)
    stepd = np.array([e.step() for _ in range(n)], np.complex64)
    e = synth("chirp", **kw)
    blocks = np.concatenate([e.steps(64) for _ in range(n // 64)])
    for y, lab in [
        (one, f"steps({n}) -- one call"),
        (blocks, f"steps(64) x {n // 64}"),
        (stepd, f"step() x {n}"),
    ]:
        ax.plot(ifreq(y) / 1e3, lw=1, label=lab)
    ax.set_xlabel("sample")
    ax.set_ylabel("instantaneous frequency (kHz)")
    ax.set_title("One chirp, three read patterns, three waveforms (F2)")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "chirp.png", dpi=110)
    plt.close(fig)


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report.

    `write=False` is the pytest path: every measurement still runs, so
    every limit is genuinely exercised, but nothing is written into the
    repo.
    """
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "Synth",
        [
            "**Pin a chirp's span, or do not use `step()` on one.** An "
            "unpinned chirp read sample-by-sample is a constant tone -- "
            f"{d.chirp_step_swept:.0f} Hz swept against "
            f"{d.chirp_steps_swept / 1e3:.0f} kHz -- and read in blocks it "
            "is a third waveform again. From Python there is no way to "
            "pin: use `Segment`/`Composer`, which pin to the segment "
            "length (§2.6, F2, gh-1115).",
            "**The SNR you ask for is the SNR you get, in all three "
            "references.** Realized noise power tracks "
            "`snr [+10log10(bps)] -10log10(span)` to "
            f"{d.snr_worst_rel * 100:.2f}% across a 15 dB sweep -- but "
            "`esno` and `ebno` differ by 10log10(bps), so on QPSK they are "
            "3.01 dB apart. Name the reference (§2.4).",
            "**`snr >= 100` is a cost switch, not an accuracy one.** It "
            "builds no AWGN child at all; one tenth of a dB below, it "
            "does. The samples on either side are indistinguishable in "
            f"float32, so the boundary shows up as {d.noisy_bytes} vs "
            f"{d.clean_bytes} bytes of state, not as visible noise (§2.4).",
            "**Pass `rrc_taps()` output raw.** The sqrt(sps) scaling is "
            f"internal and delivers unit power to "
            f"{d.rrc_power_worst * 100:.2f}% on both shaping branches, so "
            "an SNR set before shaping still means what it said. The "
            f"filter confines all but {d.rrc_oob_worst * 100:.4f}% of the "
            f"power to (1+beta)*Rs, against {d.rect_oob * 100:.1f}% "
            "leaking past the same edge unshaped (§2.5).",
            "**Everything except chirp is safe to chunk.** step() equals "
            "steps() and a block read is independent of block size on "
            "eight of nine types, and a mid-stream state hand-off resumes "
            "bit-for-bit -- which is what lets a render be split across "
            "calls, threads or processes (§2.7, §2.8).",
            "**The evidence is younger than the object.** The SNR "
            "conversion the header calls its single source of truth, four "
            "more public entry points and all ten accessors were tested by "
            "nothing until this certification; the chirp defect had been "
            "reachable from the public API the whole time (F1, F2, F3).",
        ],
    )
    if write:
        plots(d)
    R.summary(
        "\n- Raw sweeps: `data/snr_contract.csv`, "
        "`data/rrc_containment.csv`, `data/pn_autocorr.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
