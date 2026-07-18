"""async_dsss_receiver_demo.py -- Acquisition -> Dll(segments) ->
RateConverter -> MpskReceiver, the full continuous DSSS receive chain,
against a continuous Gold code carrying asynchronous BPSK data
modulation.

Stage 3 of the same multi-part story as ``dsss_acq_async_data_demo.py``
(Stage 1: does :class:`~doppler.dsss.Acquisition` land the right code
phase/Doppler bin) and ``dsss_despread_async_data_demo.py`` (Stage 2: does
that hit correctly seed :class:`~doppler.track.Dll`, and is ``segments=4``
robust enough for the DLL's *own* tracking loop). This page closes the
loop: carrier and symbol-timing recovery
(:class:`~doppler.track.MpskReceiver`) sit downstream of ``Dll``, bridged
by :class:`~doppler.resample.RateConverter`.

**Scope**: the despreader's only job is to remove the code.
``Dll(segments=4)`` (Stage 2's own tracking-optimal choice, kept for its
own robustness reasons and nothing else) emits its partial-correlation
stream at a fixed rate, a sub-multiple of the chip rate.
``RateConverter`` (arbitrary output/input ratio) converts that to a
clean ``sps=8`` -- ``MpskReceiver``'s own constructor default, the same
config used everywhere else in this codebase -- and a "normal"
``MpskReceiver(m=2, sps=8, n=4, ...)`` does carrier + symbol-timing
recovery from there. ``init_norm_freq`` is cycles per ``RateConverter``'s
*output* rate (``sps*symbol_rate``), not the despreader's own partial
rate. See ``docs/design/async-symbol-despreader.md`` §4 for why this
separation is the right architecture, not just a convenient one.

The phase-inversion hand-off (``Acquisition.code_phase`` -> ``Dll``'s
``init_chip``) is Stage 2's own finding, reused verbatim here via
:func:`doppler.dsss.handoff.dll_init_chip_from_acq`.

Two things worth knowing before reusing this pattern:

- **``MpskReceiver``'s own ``tracking``/``lock`` flags are not proof of
    correct decoding** -- always check measured BER against known data,
    extending Stage 2's caution about ``Dll.locked``.
- **``init_norm_freq`` starts from a coarse, quantized estimate.**
    ``Acquisition``'s Doppler bins are sized wide (this page's config
    resolves to a single ~kHz-scale bin at Doppler=0), so the carrier
    seed can be off by the full within-bin residual -- here, the entire
    50 Hz injected Doppler. The NDA carrier loop pulls this in over the
    first ~50 epochs (panel 4).

Four panels, all at this page's one operating point (CN0=97 dB-Hz,
chosen to unambiguously validate the pipeline mechanics rather than run
a margin sensitivity study; see Stage 2 for a page that studies margin
sensitivity instead):

1. **Decoded BPSK constellation** (settled window): two tight clusters
   at +/-1.
2. **Running BER** (settled window): flat at zero.
3. **Windowed decode correctness** (50-symbol windows, full run): stays
   at 100% throughout the run plotted here.
4. **``MpskReceiver.norm_freq`` vs. epoch**: the carrier loop's pull-in
   from the Acquisition-quantized seed to the true residual Doppler.

Run:  python -m doppler.examples.async_dsss_receiver_demo  [out.png]
"""

from __future__ import annotations

import sys
import warnings

# --8<-- [start:signal]
import numpy as np

from doppler.dsss import Acquisition
from doppler.wfm import Gold

SF = 1023  # 2**10 - 1: the CCSDS 415.0-G-1 command-link Gold code period
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2100.0  # Hz -- chips/symbol = 1428.6, non-integer (asynchronous)
SPC = 2  # samples/chip (front-end oversample)
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
TSYM = FS / SYM_RATE  # samples per symbol ~= 1.4 code epochs

DOPPLER_HZ = 50.0  # residual carrier -- never removed upstream of MpskReceiver
# Shorter than Stage 1/2's N_SYM=6000: at this margin/loop tuning the
# symbol-timing loop stays locked through ~4000 symbols and starts to
# jitter past that -- this page's asserts are sized to what's actually
# verified, not to the largest number that happens to compile.
N_SYM = 3500
PRE_SILENCE = TE * 20 + 737  # deliberately not a whole number of epochs

# CCSDS defaults: known 3-valued correlation sidelobes {-1, -65, 63}, a
# real, cross-checked reference code instead of an arbitrary MLS choice.
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def make_signal(cn0_dbhz: float, seed: int):
    """A continuous, asynchronous-symbol-clock DSSS capture at real units.

    Identical construction to Stage 1/2's ``make_signal`` -- silence,
    then the code-spread BPSK stream (independent symbol clock, residual
    carrier), with AWGN injected via the same C/N0 -> per-sample
    amplitude-SNR relationship ``Acquisition`` itself uses for sizing, so
    ``cn0_dbhz`` is directly the injected C/N0 in dB-Hz.

    Returns
    -------
    x : NDArray[np.complex64]
        The full capture (silence + signal + noise).
    data : NDArray[np.float64]
        The transmitted BPSK symbols (+/-1), ground truth for validation.
    """
    rng = np.random.default_rng(seed)
    n = int(N_SYM * TSYM) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, N_SYM + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    cph = (idx / SPC).astype(int) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)

    amp_snr = np.sqrt(10.0 ** (cn0_dbhz / 10.0) / FS)  # per-sample amplitude
    sigma = 1.0 / amp_snr  # total complex noise RMS, unit chip amplitude
    total_n = int(PRE_SILENCE) + n
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(total_n) + 1j * rng.standard_normal(total_n)
    )
    x = np.concatenate([np.zeros(int(PRE_SILENCE)), sig]).astype(
        np.complex64
    ) + noise.astype(np.complex64)
    return x, data


# --8<-- [end:signal]

from doppler.dsss.handoff import dll_init_chip_from_acq  # noqa: E402
from doppler.resample import RateConverter  # noqa: E402
from doppler.track import Dll, MpskReceiver  # noqa: E402

SEED = 6
CN0_OPERATING_DBHZ = 97.0  # deliberately strong -- see the module docstring

# Dll's own tracking-optimal segments (Stage 2) -- chosen for Dll's own
# robustness, nothing else. RateConverter bridges it to MpskReceiver's
# own sample-rate needs; the two parameters are otherwise independent.
K = 4
BN = 0.002  # every validated Dll example in this codebase uses this, not
# the constructor's own default (0.01, unstable at this geometry -- see
# Stage 2's module docstring).

MPSK_SPS = 8  # MpskReceiver's own constructor default -- a "normal" config
MPSK_N = 4


# --8<-- [start:acq_symbol_rate]
def _new_acq() -> Acquisition:
    """Symbol-rate-aware sizing, identical to Stage 2's ``_new_acq``."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            CODE,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
            symbol_rate=SYM_RATE,
            pfa=1e-3,
            pd=0.9,
        )
    assert acq.pd_predicted >= acq.pd
    return acq


# --8<-- [end:acq_symbol_rate]


def _acquire(x: np.ndarray):
    """Stream ``x`` until a hit lands; return ``(hit, hitpos, acq)``."""
    acq = _new_acq()
    frame = acq.code_bins * acq.doppler_bins
    pos = 0
    while pos + frame <= len(x):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            return hits[0], pos, acq
        pos += frame
    return None, None, acq


# --8<-- [start:handoff]
def _new_chain(chip_phase: float, doppler_hz_est: float):
    """Build a hand-off-seeded ``Dll -> RateConverter -> MpskReceiver``
    chain. ``init_norm_freq`` is cycles per ``RateConverter``'s *output*
    sample rate (``MPSK_SPS*SYM_RATE``), not the despreader's own
    partial rate."""
    partial_rate = FS * K / TE
    target_rate = MPSK_SPS * SYM_RATE
    dll = Dll(CODE, SPC, chip_phase, BN, 0.707, 0.5, segments=K)
    rc = RateConverter(rate=target_rate / partial_rate)
    rx = MpskReceiver(
        m=2,
        sps=MPSK_SPS,
        n=MPSK_N,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=doppler_hz_est / target_rate,
        warmup_syms=30,
    )
    return dll, rc, rx


# --8<-- [end:handoff]


def _receive(x, s0: int, chip_phase: float, doppler_hz_est: float):
    """Despread, resample, and demodulate ``x[s0:]`` in one pass; return
    ``(rx, syms)``. ``RateConverter.execute()`` and
    ``MpskReceiver.steps()`` are both block-size invariant (state
    carries between calls), so one call over the whole stream is
    equivalent to a per-epoch streaming call."""
    dll, rc, rx = _new_chain(chip_phase, doppler_hz_est)
    rest = x[s0:]
    nep = len(rest) // TE
    parts = [dll.steps(rest[e * TE : (e + 1) * TE]) for e in range(nep)]
    part = np.concatenate(parts)
    syms = rx.steps(rc.execute(part))
    return rx, syms


def _decode_ber(syms: np.ndarray, data: np.ndarray, data_start: int):
    """Hard-decide ``syms`` and lag-search against the known ``data`` for
    the best-matching (possibly phase-inverted) alignment. The lag search
    absorbs the Gardner/Farrow settling transient and the receiver's own
    pipeline delay -- the *existence* of a clean near-zero-error lag is
    the proof, not any particular lag value."""
    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    best_ber, best_lag, best_inv = 1.0, 0, False
    for lag in range(-100, 101):
        ti = data_start + lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(data):
            continue
        truth = data[ti]
        e_same = float(np.mean(bits[lo:hi] != truth))
        e_inv = float(np.mean(bits[lo:hi] != -truth))
        if e_same < best_ber:
            best_ber, best_lag, best_inv = e_same, lag, False
        if e_inv < best_ber:
            best_ber, best_lag, best_inv = e_inv, lag, True
    return bits, best_ber, best_lag, best_inv


def _windowed_ber(bits, data, data_start, lag, inv, window=50):
    """Fraction of incorrect decisions in each non-overlapping ``window``-
    symbol block across the *entire* recovered stream (not just the
    settled third ``_decode_ber`` uses for its headline number) -- this
    is what actually shows whether decoding stays converged for the
    whole run, since the receiver's own ``tracking``/``lock`` flags
    don't (see the module docstring)."""
    aligned = bits if not inv else -bits
    idx = data_start + lag + np.arange(len(bits))
    valid = (idx >= 0) & (idx < len(data))
    correct = np.full(len(bits), np.nan)
    correct[valid] = (aligned[valid] == data[idx[valid]]).astype(float)
    n_win = len(correct) // window
    with warnings.catch_warnings():
        # A window entirely before data_start+lag>=0 is all-NaN by
        # construction; the resulting NaN is the correct "unknown"
        # value, not a bug.
        warnings.simplefilter("ignore", RuntimeWarning)
        return np.array(
            [
                1.0 - np.nanmean(correct[i * window : (i + 1) * window])
                for i in range(n_win)
            ]
        )


def _norm_freq_trace(x, s0, chip_phase, doppler_hz_est, n_epochs):
    """Stream the chain epoch by epoch, recording ``rx.norm_freq`` after
    each call -- the carrier loop's pull-in trace from its
    Acquisition-quantized seed toward the true residual Doppler."""
    dll, rc, rx = _new_chain(chip_phase, doppler_hz_est)
    nf = np.full(n_epochs, np.nan)
    pos = s0
    for i in range(n_epochs):
        if pos + TE > len(x):
            break
        part = dll.steps(x[pos : pos + TE])
        pos += TE
        stream = rc.execute(part)
        if len(stream):
            rx.steps(stream)
        nf[i] = rx.norm_freq
    return nf


def main(out_path: str = "async_dsss_receiver_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, data = make_signal(CN0_OPERATING_DBHZ, SEED)

    # --- stage 1: acquisition -- stream until a hit lands -------------------
    hit, hitpos, acq = _acquire(x)
    assert hit is not None, "acquisition failed to find the continuous code"
    dop_bin, code_phase, _pk, _noise_est, test_stat, cn0_dbhz_est, *_rest = hit
    print(
        f"acquired: doppler_bin={dop_bin} code_phase={code_phase} "
        f"test_stat={test_stat:.1f} cn0_dbhz_est={cn0_dbhz_est:.1f} "
        f"(injected {CN0_OPERATING_DBHZ:.1f})"
    )

    # --- stage 2: hand-off ---------------------------------------------------
    doppler_bins = acq.doppler_bins
    k_fold = (dop_bin + doppler_bins // 2) % doppler_bins - doppler_bins // 2
    doppler_hz_est = k_fold * acq.doppler_res_hz
    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    frame = acq.code_bins * acq.doppler_bins
    s0 = hitpos + frame
    data_start = round((s0 - PRE_SILENCE) / TSYM)

    # --- stage 3: despread -> resample -> demod ------------------------------
    rx, syms = _receive(x, s0, chip_phase, doppler_hz_est)
    bits, ber, lag, inv = _decode_ber(syms, data, data_start)
    print(
        f"segments={K} sps={MPSK_SPS}: recovered {len(syms)} symbols, "
        f"tracking={rx.tracking} lock={rx.lock:.2f} ber={ber:.4f} "
        f"lag={lag} inverted={inv}"
    )
    assert ber < 0.01, "failed to decode cleanly"
    assert rx.tracking == 1, "MpskReceiver never handed over to tracking"

    wber = _windowed_ber(bits, data, data_start, lag, inv)
    assert np.nanmean(wber[-5:]) < 0.05, (
        "expected windowed BER to stay converged through the end of the "
        f"run (last 5 windows mean={np.nanmean(wber[-5:]):.3f})"
    )

    n_epochs_nf = min(300, (len(x) - s0) // TE)
    nf_trace = _norm_freq_trace(x, s0, chip_phase, doppler_hz_est, n_epochs_nf)
    norm_freq_true = DOPPLER_HZ / (MPSK_SPS * SYM_RATE)

    # --- plot -----------------------------------------------------------
    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    a.scatter(
        syms[lo:hi].real, syms[lo:hi].imag, s=8, color="#1f77b4", alpha=0.5
    )
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    lim = 1.3 * np.max(np.abs(syms[lo:hi]))
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.set_title(
        f"Decoded BPSK symbols (settled window)\nBER={ber:.4f}", fontsize=9
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    aligned = bits if not inv else -bits
    idx = data_start + lag + np.arange(lo, hi)
    correct = aligned[lo:hi] == data[idx]
    running_ber = 1.0 - np.cumsum(correct) / (np.arange(len(correct)) + 1)
    b.plot(running_ber, color="#2ca02c", lw=1.1)
    b.set_ylim(0, max(0.05, running_ber.max() * 1.1))
    b.set_title("Running BER over the settled window", fontsize=9)
    b.set_xlabel("symbol index (settled window)")
    b.set_ylabel("running BER")
    b.grid(alpha=0.25)

    c.plot(np.arange(len(wber)) * 50, wber, lw=1.1, color="#1f77b4")
    c.axhline(0.5, color="k", lw=0.8, ls="--", label="chance")
    c.set_ylim(-0.05, 0.65)
    c.set_title(
        "Windowed decode correctness, full run\n(50-symbol windows)",
        fontsize=9,
    )
    c.set_xlabel("symbol index")
    c.set_ylabel("windowed BER")
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

    ep = np.arange(len(nf_trace))
    d.plot(ep, nf_trace, lw=1.1, color="#1f77b4", label="rx.norm_freq")
    d.axhline(
        norm_freq_true, color="k", lw=1.0, ls="--", label="true norm_freq"
    )
    d.set_title(
        "Carrier pull-in from the Acquisition-quantized seed", fontsize=9
    )
    d.set_xlabel("epoch")
    d.set_ylabel("MpskReceiver.norm_freq")
    d.legend(fontsize=7)
    d.grid(alpha=0.25)

    fig.suptitle(
        f"Acquisition -> Dll(segments={K}) -> RateConverter -> "
        f"MpskReceiver(sps={MPSK_SPS}) -- CCSDS Gold code (SF={SF}), "
        f"continuous asynchronous BPSK data ({SYM_RATE:.0f} sym/s), "
        f"C/N0={CN0_OPERATING_DBHZ:.0f} dB-Hz",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_dsss_receiver_demo.png")
