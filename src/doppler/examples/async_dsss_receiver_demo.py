"""async_dsss_receiver_demo.py — Acquisition -> Dll(segments) -> MpskReceiver,
the full continuous DSSS receive chain, at real physical parameters.

A **continuous** (not bursty) 1023-chip PN code at 3 Mchips/s carries BPSK
data at 2100 sym/s. ``chips/symbol = 3e6/2100 ~= 1428.6`` is not an integer,
so the data-symbol clock is **asynchronous** to the code-epoch clock — the
same two-clock problem :mod:`async_despread_demo`/``docs/design/
async-symbol-despreader.md`` study, just at real units with a genuine
:class:`~doppler.dsss.Acquisition` search in front of it (both existing
examples construct :class:`~doppler.track.Dll` already phase-locked).

``MpskReceiver``'s own docstring states the intended composition: *"A
DSSS-MPSK receiver is ``Dll(segments) -> MpskReceiver``: despread to
symbol-rate soft chips, then this modem."* This is the first end-to-end
validation of that composition with a real acquisition front end.

Pipeline:
  1. :class:`~doppler.dsss.Acquisition` streams raw samples (no
     ``reset()``-hopping needed — the code is always present, unlike a
     sparse burst) until it reports a hit.
  2. The hit's ``doppler_bin``/``code_phase`` seed :class:`~doppler.track.Dll`
     (segments mode: ``K`` non-coherent partial correlations per code epoch)
     and :class:`~doppler.track.MpskReceiver` (carrier NCO).
  3. ``Dll.steps()``'s partial stream feeds directly into
     ``MpskReceiver.steps()``, which matched-filters (boxcar), recovers the
     carrier (NDA, cold-start capable) and symbol timing (Gardner+Farrow),
     and emits decoded BPSK symbols.

Four non-obvious findings from getting this to actually lock, all worth
knowing before reusing this pattern:

1. **``K`` needs to be much larger than the DLL-only "sweet spot".**
   ``docs/design/async-symbol-despreader.md`` finds ``K=4`` optimal — but
   that's tuned for the DLL's *own* non-coherent code-discriminator
   variance, not for feeding a downstream matched filter. Each partial is
   only ``K``-times weaker than a full coherent epoch, so ``MpskReceiver``'s
   own boxcar (length ``sps``) needs enough partials to rebuild real
   coherent gain. ``K=4`` never locked here; ``K=34`` (chosen so
   ``round(K * T_sym/T_epoch)`` lands on an ``MpskReceiver``-friendly ``sps``)
   locks robustly.
2. **``Acquisition.code_phase`` and ``Dll``'s ``init_chip`` are phase-
   *inverted*.** ``code_phase`` is a correlation *lag* (how far the
   reference must roll to match the capture); ``Dll``'s seed is the code's
   actual current phase — the conversion is
   ``chip_phase = (sf - code_phase/spc) % sf``, not ``code_phase/spc``
   directly. Using the naive (un-inverted) formula despreads to pure noise.
3. **``MpskReceiver``'s ``init_norm_freq`` is cycles per *its own input
   sample*** (the ``Dll`` partial stream), not cycles per raw ADC sample —
   convert via ``doppler_hz / (fs * K / T_epoch)``, the partial rate, or a
   real residual Doppler seeds a wildly wrong carrier frequency.
4. **``Dll.locked`` never latches here despite a clean, verified decode.**
   ``Dll.locked`` genuinely works in general (see the
   :class:`~doppler.track.Dll`-``Costas``-``SymbolSync`` chain in
   ``receiver-lock.md``, converged and observed via telemetry) — but its
   default lock sizing (``configure_lock(pfa=1e-3, n_looks=20)``) is not
   re-tuned for this large a ``K`` here, where each individual partial's
   coherent gain is thin. Don't take ``Dll.locked`` on faith for an
   unfamiliar ``K``; check ``MpskReceiver.tracking``/measured BER (or
   re-tune ``configure_lock``) instead.

Run:  python -m doppler.examples.async_dsss_receiver_demo  [out.png]
"""

from __future__ import annotations

import sys
import warnings

# --8<-- [start:signal]
import numpy as np

from doppler.dsss import Acquisition
from doppler.track import Dll, MpskReceiver
from doppler.wfm import PN, mls_poly

NMLS = 10  # 2**10 - 1 = 1023-chip MLS code
SF = (1 << NMLS) - 1
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2100.0  # Hz -- chips/symbol = 1428.6, non-integer (asynchronous)
SPC = 2  # samples/chip (front-end oversample)
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
TSYM = FS / SYM_RATE  # samples per symbol ~= 1.4 code epochs

DOPPLER_HZ = 50.0  # modest residual carrier after any coarse pre-correction
N_SYM = 6000
PRE_SILENCE = TE * 20 + 737  # deliberately not a whole number of epochs

CODE = np.asarray(
    PN(poly=mls_poly(NMLS), seed=1, length=NMLS).generate(SF)
).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def make_signal(cn0_dbhz: float, seed: int = 3):
    """A continuous, asynchronous-symbol-clock DSSS capture at real units.

    Silence, then the code-spread BPSK stream (independent symbol clock,
    residual carrier), with AWGN injected via the same C/N0 -> per-sample
    amplitude-SNR relationship ``Acquisition`` itself uses for sizing, so
    ``cn0_dbhz`` is directly the injected C/N0 in dB-Hz (bandwidth/
    integration-time independent, unlike a raw per-sample or symbol SNR).

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

# Segments per code epoch (finding #1 above) and the MpskReceiver samples/
# symbol it implies -- chosen so round(K*TSYM/TE) lands on a value with a
# small divisor for MpskReceiver's carrier-arm count `n`.
K = 34
SPS_MPSK = round(K * TSYM / TE)  # 47
N_MPSK = next(c for c in (4, 2, 1) if SPS_MPSK % c == 0)  # 1

# The operating point below is deliberately strong (not a sensitivity study)
# to unambiguously validate the pipeline mechanics -- see the module
# docstring. It reads as a high dB-Hz figure because C/N0 is normalised by
# the front-end sample rate (FS=6 MHz here): a fixed C/N0 buys much less
# per-raw-sample SNR at a high sample rate than at a narrowband one.
CN0_OPERATING_DBHZ = 97.0


def main(out_path: str = "async_dsss_receiver_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, data = make_signal(CN0_OPERATING_DBHZ)

    # --- stage 1: acquisition -- stream until a hit lands ------------------
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            CODE,
            reps=4,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            pfa=1e-3,
            pd=0.9,
        )
    frame = acq.code_bins * acq.doppler_bins
    hit = None
    pos = 0
    while pos + frame <= len(x):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            hit, hitpos = hits[0], pos
            break
        pos += frame
    assert hit is not None, "acquisition failed to find the continuous code"
    dop_bin, code_phase, _peak_mag, _noise_est, test_stat, cn0_dbhz_est = hit
    print(
        f"acquired: doppler_bin={dop_bin} code_phase={code_phase} "
        f"test_stat={test_stat:.1f} cn0_dbhz_est={cn0_dbhz_est:.1f} "
        f"(injected {CN0_OPERATING_DBHZ:.1f})"
    )

    # --- stage 2: hand-off (findings #2, #3 above) --------------------------
    doppler_bins = acq.doppler_bins
    k_fold = (dop_bin + doppler_bins // 2) % doppler_bins - doppler_bins // 2
    doppler_hz_est = k_fold * acq.doppler_res_hz
    partial_rate = FS * K / TE
    norm_freq = doppler_hz_est / partial_rate  # MpskReceiver's own units
    chip_phase = (SF - code_phase / SPC) % SF  # inverted vs. code_phase/SPC

    rest = x[hitpos + frame :]

    # --- stage 3: despread (Dll, segments=K) --------------------------------
    dll = Dll(CODE, SPC, chip_phase, 0.002, 0.707, 0.5, segments=K)
    nep = len(rest) // TE
    parts = [dll.steps(rest[e * TE : (e + 1) * TE]) for e in range(nep)]
    part = np.concatenate(parts)
    print(
        f"despread: {len(part)} partials, code_rate={dll.code_rate:.6f} "
        f"(1.0 = perfectly tracked)"
    )

    # --- stage 4: carrier + symbol recovery (MpskReceiver) ------------------
    rx = MpskReceiver(
        m=2,
        sps=SPS_MPSK,
        n=N_MPSK,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=norm_freq,
        warmup_syms=30,
    )
    syms = rx.steps(part)
    print(
        f"recovered {len(syms)} symbols, tracking={rx.tracking} "
        f"lock={rx.lock:.2f} (dll.locked={dll.locked} -- see finding #4)"
    )

    # --- validate: decode and compare against the known transmitted data ---
    # A lag search absorbs the Gardner/Farrow settling transient and the
    # receiver's own pipeline delay (test_mpsk_receiver.py's _ser() does the
    # same) -- the *existence* of a clean near-zero-error lag is the proof,
    # not any particular lag value.
    bits = np.where(syms.real > 0, 1.0, -1.0)
    data_start = round((hitpos + frame - PRE_SILENCE) / TSYM)
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
    print(
        f"decoded BER={best_ber:.4f} at lag={best_lag} inverted={best_inv} "
        f"(absolute-phase ambiguity, resolved by a sync word downstream "
        f"in a real link)"
    )
    assert best_ber < 0.01, "pipeline failed to decode the known data cleanly"
    assert rx.tracking == 1, "MpskReceiver never handed over to tracking"

    # --- plot ---------------------------------------------------------------
    fig, (a, b) = plt.subplots(1, 2, figsize=(10, 4.4))
    off = data_start + best_lag
    aligned = np.sign(syms.real) if not best_inv else -np.sign(syms.real)
    correct = aligned[lo:hi] == data[off + np.arange(lo, hi)][: hi - lo]
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
        f"Decoded BPSK symbols (settled window)\nBER={best_ber:.4f}",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    running_ber = 1.0 - np.cumsum(correct) / (np.arange(len(correct)) + 1)
    b.plot(running_ber, color="#2ca02c", lw=1.1)
    b.set_ylim(0, max(0.05, running_ber.max() * 1.1))
    b.set_title("Running BER over the settled window", fontsize=9)
    b.set_xlabel("symbol index (settled window)")
    b.set_ylabel("running BER")
    b.grid(alpha=0.25)

    fig.suptitle(
        f"Acquisition -> Dll(segments={K}) -> MpskReceiver(sps={SPS_MPSK}) — "
        f"1023-chip code @ {CHIP_RATE / 1e6:.0f} Mcps, {SYM_RATE:.0f} sym/s "
        f"(async)",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_dsss_receiver_demo.png")
