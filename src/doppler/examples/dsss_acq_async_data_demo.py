"""dsss_acq_async_data_demo.py -- Acquisition alone, against a continuous
Gold code carrying asynchronous BPSK data modulation.

Stage 1 of a multi-part story ("Acquisition -> Dll(segments) ->
MpskReceiver" is the full chain, told one validated stage at a time
instead of as a single end-of-run BER number). This page asks the
narrowest possible question: with a **continuous** (non-bursty) 1023-chip
Gold code at 3 Mchips/s carrying BPSK data at 2100 sym/s --
chips/symbol = 3e6/2100 ~= 1428.6, not an integer, so the data-symbol
clock is asynchronous to the code-epoch clock -- does
:class:`~doppler.dsss.Acquisition` still land on the *exact* right code
phase and Doppler bin, and does its per-epoch test statistic stay safely
above the CFAR gate even when a data-bit transition falls mid-epoch?

Four panels:

1. **Chip-level zoom** right at the hit: the raw signal (genie-derotated
   by the exactly-known injected Doppler) overlaid with the TRUE
   transmitted chip*data sequence and Acquisition's own reported
   chip-phase reconstruction. Bit-for-bit agreement here is the actual
   proof -- Acquisition's ``code_phase``/``doppler_bin`` estimate is
   exactly right, not merely close.
2. **Test statistic vs. epoch, Monte-Carlo over random data, code phase,
   and a +-100 Hz residual Doppler.** ``doppler_bins=1`` at this operating
   point, so each ``push()`` evaluates exactly one code epoch -- a direct
   per-epoch window onto how asynchronous data modulation affects the
   search, generalized across a real frequency-uncertainty range rather
   than one fixed offset.
3. **Code-phase error vs. epoch, same sweep.** Near-zero almost
   everywhere, with occasional gross (hundreds-of-chips) mislocks at the
   same epochs where the test statistic dipped.
4. **Doppler error vs. epoch.** A flat-vs-time band spanning the full
   +-100 Hz trial-to-trial spread (each individual trial's own Doppler is
   constant across its epochs) -- ``doppler_bins=1`` means this operating
   point makes no attempt at fine Doppler resolution at all (that's a
   downstream tracking-loop job).

**Why the mislocks happen (verified, not just observed):** a data-bit
transition landing mid-epoch splits that epoch's coherent sum into two
oppositely-signed partial segments. At the *true* code phase this costs
correlation gain (the two segments partially cancel). The Gold code's
3-valued *full-period* correlation bound ({-1, -65, 63} off-peak) does
**not** apply to these unequal-length *partial*-period segments -- at some
*other* candidate phase, the two mis-signed partial sums can happen to add
constructively instead, producing a peak that beats the (already
weakened) true-phase peak. This is confirmed directly below by replaying
one identified bad epoch with **all injected noise removed**: the
identical mislock reproduces bit-for-bit, proving it is a deterministic
consequence of the code's partial-window self-correlation structure and
the transition's position within the epoch -- not a noise event.

**A second figure asks the same question at a realistic link margin**
(Es/N0 = 10 dB, not the deliberately-strong 97 dB-Hz above): detection
probability vs. integration depth, coherent (one D-epoch dump) vs.
non-coherent (D independent 1-epoch looks), same total epochs either way.
At this margin the effect isn't an occasional mislock anymore -- it's a
first-order Pd gap. An 11-epoch coherent window spans ~8 symbols (~7 data
transitions), and coherent Pd plateaus around 68%, far short of its own
91% theoretical target, while non-coherent (immune to phase disagreement
between looks) comfortably clears 99%.

**Both curves get an overlaid theoretical prediction** -- a
semi-analytical Pd(D) computed from the exact chip-timing combinatorics
of a uniformly-distributed data-bit transition (window phase quadrature x
exact enumeration over the i.i.d. +-1 data signs touched, no noise Monte
Carlo), fed through the same ``det_pd``/``marcum_q`` primitives
:class:`~doppler.dsss.Acquisition` itself sizes against. The non-coherent
theory matches its empirical curve almost exactly (each look is
independent, so summing non-centralities is exact). The coherent theory
-- accounting for the engine's slow-time Doppler FFT leaking a
transition's energy into non-zero bins, not just DC -- captures most of
the effect but still undershoots empirical Pd by a residual amount,
because it doesn't model the analogous leakage on the *code-phase* axis
(the same mislock mechanism from the first figure, now also handing out
partial credit toward detection, not just occasional wrong-phase locks).

Downstream despread (``Dll(segments)`` -- Stage 2, see
``dsss_despread_async_data_demo.py``) and demod (``MpskReceiver`` --
Stage 3, see ``async_dsss_receiver_demo.py``) are later stages of this
story -- this page is deliberately acquisition-only.

Run:  python -m doppler.examples.dsss_acq_async_data_demo  [out.png]
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

DOPPLER_HZ = 50.0  # modest residual carrier after any coarse pre-correction
N_SYM = 6000
PRE_SILENCE = TE * 20 + 737  # deliberately not a whole number of epochs

# CCSDS defaults: known 3-valued correlation sidelobes {-1, -65, 63}, a
# real, cross-checked reference code instead of an arbitrary MLS choice.
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def make_signal(cn0_dbhz: float, seed: int):
    """A continuous, asynchronous-symbol-clock DSSS capture at real units.

    Silence, then the code-spread BPSK stream (independent symbol clock,
    residual carrier), with AWGN injected via the same C/N0 -> per-sample
    amplitude-SNR relationship ``Acquisition`` itself uses for sizing, so
    ``cn0_dbhz`` is directly the injected C/N0 in dB-Hz.

    Returns
    -------
    x : NDArray[np.complex64]
        The full capture (silence + signal + noise).
    data : NDArray[np.float64]
        The transmitted BPSK symbols (+/-1), ground truth for validation.
    si, cph : NDArray[np.intp]
        Per-sample symbol index and chip-phase index used to build ``x``,
        relative to the start of ``sig`` (i.e. *excluding* the prepended
        silence) -- ground truth for the chip-level check.
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
    return x, data, si, cph


# --8<-- [end:signal]

from doppler.detection import det_pd, marcum_q  # noqa: E402 -- theory below
from doppler.dsss.handoff import (  # noqa: E402 -- not needed by make_signal
    dll_init_chip_from_acq,
)

# Deliberately strong (not a sensitivity study) to unambiguously validate
# the search mechanics -- see the module docstring. It reads as a high
# dB-Hz figure because C/N0 is normalised by the front-end sample rate
# (FS=6 MHz here): a fixed C/N0 buys much less per-raw-sample SNR at a
# high sample rate than at a narrowband one.
CN0_OPERATING_DBHZ = 97.0
SEED = 6

N_EPOCHS_MC = 100  # Monte-Carlo observation window, in code epochs
N_TRIALS_MC = 200  # independent random-data/random-code-phase trials
DOPPLER_UNCERTAINTY_HZ = 100.0  # Monte-Carlo trials draw from +-this, uniform


def _new_acq() -> Acquisition:
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        return Acquisition(
            CODE,
            reps=4,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            pfa=1e-3,
            pd=0.9,
        )


def _mc_trial(trial: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Test statistic / code-phase-error / Doppler-error traces for one
    random-data/random-phase/random-Doppler trial.

    Builds ``N_EPOCHS_MC`` epochs of continuous signal (no silence --
    this experiment is about the search's per-epoch behaviour once
    already in steady transmission, not acquisition latency), random
    data, a random starting code phase, and a residual Doppler drawn
    uniformly from ``+-DOPPLER_UNCERTAINTY_HZ`` (so the mislock-mechanism
    finding isn't an artifact of one fixed offset), and streams it
    through a fresh ``Acquisition`` instance one epoch (one ``push()``)
    at a time. Returns three length-``N_EPOCHS_MC`` arrays (test
    statistic, code-phase error in chips, Doppler error in Hz), NaN
    where that epoch's peak fell below the CFAR gate (a real miss, not
    a bug).
    """
    rng = np.random.default_rng(1000 + trial)
    n = N_EPOCHS_MC * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, int(n / TSYM) + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    phase0 = int(rng.integers(0, SF))
    doppler_hz = float(
        rng.uniform(-DOPPLER_UNCERTAINTY_HZ, DOPPLER_UNCERTAINTY_HZ)
    )
    cph = ((idx / SPC).astype(int) + phase0) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (doppler_hz / FS) * idx)
    amp_snr = np.sqrt(10.0 ** (CN0_OPERATING_DBHZ / 10.0) / FS)
    sigma = 1.0 / amp_snr
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )
    x = (sig + noise).astype(np.complex64)

    acq = _new_acq()
    frame = acq.code_bins * acq.doppler_bins
    assert frame == TE, "expected doppler_bins=1 (one epoch per push)"
    doppler_bins = acq.doppler_bins
    doppler_res_hz = acq.doppler_res_hz
    nframes = len(x) // frame
    ts = np.full(nframes, np.nan)
    code_err = np.full(nframes, np.nan)  # chips, circular, vs. true phase0
    dopp_err = np.full(nframes, np.nan)  # Hz, vs. true doppler_hz
    pos = 0
    for i in range(nframes):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            dop_bin, code_phase, _pk, _n, test_stat, _c, *_rest = hits[0]
            ts[i] = test_stat
            chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
            code_err[i] = ((chip_phase - phase0 + SF / 2) % SF) - SF / 2
            k_fold = (
                dop_bin + doppler_bins // 2
            ) % doppler_bins - doppler_bins // 2
            dopp_err[i] = k_fold * doppler_res_hz - doppler_hz
        pos += frame
    return ts, code_err, dopp_err


def _replay_epoch_noiseless(trial: int, epoch: int) -> float:
    """Rebuild one Monte-Carlo (trial, epoch) with all injected noise
    removed and report its code-phase error in chips.

    Uses the exact same ``rng``/``phase0``/``doppler_hz`` construction as
    ``_mc_trial`` for that trial (same draw order), so this reproduces the
    identical code phase, data sequence, Doppler, and data-bit-transition
    timing -- only the AWGN term is dropped. If the mislock survives
    noiseless, it is a deterministic property of the code and the
    transition's position within the epoch, not a noise event.
    """
    rng = np.random.default_rng(1000 + trial)
    n = N_EPOCHS_MC * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, int(n / TSYM) + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    phase0 = int(rng.integers(0, SF))
    doppler_hz = float(
        rng.uniform(-DOPPLER_UNCERTAINTY_HZ, DOPPLER_UNCERTAINTY_HZ)
    )
    cph = ((idx / SPC).astype(int) + phase0) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (doppler_hz / FS) * idx)
    e0, e1 = epoch * TE, (epoch + 1) * TE
    ep_clean = sig[e0:e1].astype(np.complex64)  # no noise added

    acq = _new_acq()
    hits = acq.push(ep_clean)
    assert hits, "noiseless epoch produced no detection at all"
    _dop_bin, code_phase, _pk, _n, _ts, _c, *_rest = hits[0]
    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    return float(((chip_phase - phase0 + SF / 2) % SF) - SF / 2)


# --8<-- [start:diversity_configs]
# A realistic link margin, unlike CN0_OPERATING_DBHZ (97 dB-Hz, deliberately
# strong for the mislock-mechanism story above): Es/N0 = C/N0 - 10*log10(Rs),
# so C/N0 = Es/N0 + 10*log10(symbol_rate). Pd vs. integration depth, coherent
# vs. non-coherent: for depth D, "coherent" forces reps=D, max_noncoh=1 (a
# single D-epoch coherent dump) and "non-coherent" forces reps=1,
# max_noncoh=D (D independent 1-epoch looks, power-summed). auto-config
# picks the *smallest* depth that meets pd=0.9 within the ceiling you give
# it, so for D below what's needed it's forced to use the full D (still
# short of pd) -- sweeping D traces out the actual Pd-vs-integration curve
# for each strategy, not just its endpoint.
DIVERSITY_ES_N0_DB = 10.0
DIVERSITY_CN0_DBHZ = DIVERSITY_ES_N0_DB + 10 * np.log10(SYM_RATE)  # ~43.2
N_EPOCHS_DIV = 96  # epochs per trial; >= the deepest depth swept below
DEPTHS = list(range(1, 13))  # epochs integrated; brackets the ~11-epoch knee
# --8<-- [end:diversity_configs]


# --8<-- [start:diversity_acq]
def _new_acq_config(
    cn0_dbhz: float, reps: int, max_noncoh: int
) -> Acquisition:
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        return Acquisition(
            CODE,
            reps=reps,  # coherent-depth ceiling (1 = never grow it)
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=cn0_dbhz,  # sizing target -- pushes past the coherent
            pfa=1e-3,  # ceiling to engage max_noncoh
            pd=0.9,
            max_noncoh=max_noncoh,  # non-coherent looks the engine may add
        )


# --8<-- [end:diversity_acq]


def _diversity_trial(trial: int, cn0_dbhz: float, reps: int, max_noncoh: int):
    """Test-stat and code-phase-error trace for one trial of one
    epoch-diversity configuration.

    Unlike ``_mc_trial`` (fixed at ``doppler_bins=1``), this drives
    ``reps``/``max_noncoh`` directly, so ``frame`` may span more than one
    epoch (a multi-epoch coherent dump) or a decision may only land every
    ``n_noncoh`` pushes (a non-coherent combine) -- the column index of
    the returned arrays is therefore in units of "one push", not "one
    epoch"; the caller rescales by ``acq.doppler_bins`` to get real time.
    Also draws a residual Doppler uniformly from
    ``+-DOPPLER_UNCERTAINTY_HZ`` per trial, same as ``_mc_trial``, so the
    epoch-diversity comparison isn't tied to one fixed offset either.
    """
    rng = np.random.default_rng(1000 + trial)
    n = N_EPOCHS_DIV * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, int(n / TSYM) + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    phase0 = int(rng.integers(0, SF))
    doppler_hz = float(
        rng.uniform(-DOPPLER_UNCERTAINTY_HZ, DOPPLER_UNCERTAINTY_HZ)
    )
    cph = ((idx / SPC).astype(int) + phase0) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (doppler_hz / FS) * idx)
    # The real injected signal, not CN0_OPERATING_DBHZ (that constant is the
    # deliberately-strong mislock-mechanism story above) -- this sweep is a
    # genuine link-margin study at DIVERSITY_ES_N0_DB.
    amp_snr = np.sqrt(10.0 ** (DIVERSITY_CN0_DBHZ / 10.0) / FS)
    sigma = 1.0 / amp_snr
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )
    x = (sig + noise).astype(np.complex64)

    acq = _new_acq_config(cn0_dbhz, reps, max_noncoh)
    frame = acq.code_bins * acq.doppler_bins
    nframes = len(x) // frame
    ts = np.full(nframes, np.nan)
    code_err = np.full(nframes, np.nan)
    pos = 0
    for i in range(nframes):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            _dop, code_phase, _pk, _n, test_stat, _c, *_rest = hits[0]
            ts[i] = test_stat
            chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
            code_err[i] = ((chip_phase - phase0 + SF / 2) % SF) - SF / 2
        pos += frame
    return ts, code_err, acq


def _diversity_sweep(cn0_dbhz: float, reps: int, max_noncoh: int, label: str):
    """Aggregate ``N_TRIALS_MC`` trials of one epoch-diversity config."""
    ts_rows, ce_rows = [], []
    acq_ref = None
    for tr in range(N_TRIALS_MC):
        ts, ce, acq_ref = _diversity_trial(tr, cn0_dbhz, reps, max_noncoh)
        ts_rows.append(ts)
        ce_rows.append(ce)
    ts_mat = np.array(ts_rows)
    ce_mat = np.array(ce_rows)
    # exclude non-coherent's still-accumulating (NaN) slots from the rate
    valid = ~np.isnan(ce_mat)
    mislock = valid & (np.abs(np.nan_to_num(ce_mat)) > 5.0)
    mislock_rate = float(np.sum(mislock) / np.sum(valid))

    # Empirical Pd -- the metric that actually discriminates at low margin:
    # push() only ever reports a value once it clears threshold, so "min
    # test statistic among detections" degenerates toward the threshold
    # itself when Pd is low (a handful of trials get lucky, and that's all
    # you ever see). Opportunities/trial is push-count for the coherent
    # path (one decision per push) but push-count // n_noncoh for the
    # non-coherent path (a decision only completes every n_noncoh pushes;
    # ts_mat's column count is still per-push, not per-decision).
    opportunities_per_trial = ts_mat.shape[1] // acq_ref.n_noncoh
    total_opportunities = N_TRIALS_MC * opportunities_per_trial
    pd_empirical = float(np.sum(valid) / total_opportunities)

    # `.threshold` (mean-CFAR scale) is what test_stat is plotted against;
    # `.eta`/`.eta_nc` (raw Rayleigh/Marcum scale) is what det_pd/marcum_q
    # need for the theoretical model below -- two different quantities.
    thr = acq_ref.eta_nc if acq_ref.n_noncoh > 1 else acq_ref.threshold
    eta_raw = acq_ref.eta_nc if acq_ref.n_noncoh > 1 else acq_ref.eta
    print(
        f"epoch-diversity[{label}]: doppler_bins={acq_ref.doppler_bins} "
        f"n_noncoh={acq_ref.n_noncoh} threshold={thr:.2f} "
        f"pd_empirical={pd_empirical:.4f} (pd_predicted="
        f"{acq_ref.pd_predicted:.4f}) mislock_rate={mislock_rate:.4f} "
        f"({int(np.sum(mislock))} of {int(np.sum(valid))})"
    )
    return (
        ts_mat,
        ce_mat,
        thr,
        acq_ref.doppler_bins,
        acq_ref.n_noncoh,
        mislock_rate,
        pd_empirical,
        acq_ref.pd_predicted,
        eta_raw,
    )


EPOCHS_PER_SYMBOL = TSYM / TE  # ~1.396; fixed by the waveform, not random


def _window_epoch_segments(phi0: float, depth: int):
    """Segment the ``depth``-epoch window starting at symbol-phase
    ``phi0`` (elapsed into the current data symbol, in ``[0,
    EPOCHS_PER_SYMBOL)``) into per-epoch ``[(length, symbol_id), ...]``
    lists. ``symbol_id`` increments at each crossed data-bit boundary, so
    two segments sharing a ``symbol_id`` share the same (unknown, i.i.d.
    +-1) data sign -- this is the exact combinatorial structure a
    uniformly-distributed transition (or several, for a wide window)
    imposes on the window, with no noise or Monte Carlo involved.
    """
    segments_per_epoch = []
    phase = phi0
    symbol_id = 0
    for _ in range(depth):
        segs = []
        remaining = 1.0
        while remaining > 1e-9:
            to_boundary = EPOCHS_PER_SYMBOL - phase
            take = min(to_boundary, remaining)
            segs.append((take, symbol_id))
            remaining -= take
            phase += take
            if phase >= EPOCHS_PER_SYMBOL - 1e-9:
                phase = 0.0
                symbol_id += 1
        segments_per_epoch.append(segs)
    return segments_per_epoch, symbol_id + 1


def _theoretical_pd(depth, snr, code_bins, eta, n_noncoh, n_phi=200):
    """Semi-analytical Pd(depth) with a uniformly-distributed data-bit
    transition inside the integration window -- no AWGN Monte Carlo, no
    ``Acquisition`` simulation, just the exact chip-timing combinatorics
    (phase quadrature over the window's position relative to the symbol
    clock, exact enumeration over the small number of i.i.d. +-1 data
    signs touched) fed through the same ``det_pd``/``marcum_q`` primitives
    ``Acquisition`` itself sizes against.

    Coherent (``n_noncoh<=1``): the engine's slow-time FFT searches every
    Doppler bin, and a mid-window phase step (from a transition) leaks
    energy into non-zero bins too, so the right per-window amplitude is
    the *peak* across the window's own D-point DFT, not just its DC term.
    Non-coherent (``n_noncoh>1``): each look is independent, and summing
    independent non-central chi-squared(2) terms gives a non-central
    chi-squared(2*n_noncoh) whose non-centrality is the *sum* of the
    individual looks' non-centralities -- exactly what ``marcum_q``
    expects.
    """
    phis = (np.arange(n_phi) + 0.5) / n_phi * EPOCHS_PER_SYMBOL
    pd_acc = 0.0
    for phi0 in phis:
        segs_per_epoch, n_symbols = _window_epoch_segments(phi0, depth)
        n_combos = 1 << n_symbols
        pd_sum = 0.0
        for bits in range(n_combos):
            signs = np.array(
                [1.0 if (bits >> i) & 1 else -1.0 for i in range(n_symbols)]
            )
            v = np.array(
                [
                    sum(signs[sid] * length for length, sid in segs)
                    for segs in segs_per_epoch
                ]
            )
            if n_noncoh <= 1:
                if depth == 1:
                    alpha = abs(v[0])
                else:
                    alpha = np.abs(np.fft.fft(v)).max() / depth
                pd_sum += det_pd(snr * alpha, depth * code_bins, eta)
            else:
                lam = sum(
                    (np.sqrt(2.0 * code_bins) * snr * abs(vi)) ** 2 for vi in v
                )
                pd_sum += marcum_q(n_noncoh, np.sqrt(lam), eta)
        pd_acc += pd_sum / n_combos
    return pd_acc / n_phi


def main(out_path: str = "dsss_acq_async_data_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, data, si, cph = make_signal(CN0_OPERATING_DBHZ, SEED)

    # --- acquisition: stream until a hit lands ------------------------------
    acq = _new_acq()
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
    (
        dop_bin,
        code_phase,
        _peak_mag,
        _noise_est,
        test_stat,
        cn0_dbhz_est,
        *_rest,
    ) = hit
    print(
        f"acquired: hitpos={hitpos} doppler_bin={dop_bin} "
        f"code_phase={code_phase} test_stat={test_stat:.1f} "
        f"cn0_dbhz_est={cn0_dbhz_est:.1f} (injected {CN0_OPERATING_DBHZ:.1f})"
    )

    doppler_bins = acq.doppler_bins
    k_fold = (dop_bin + doppler_bins // 2) % doppler_bins - doppler_bins // 2
    doppler_hz_est = k_fold * acq.doppler_res_hz
    assert abs(doppler_hz_est - DOPPLER_HZ) <= acq.doppler_res_hz, (
        f"acquisition's Doppler estimate ({doppler_hz_est:.1f} Hz) missed "
        f"the injected {DOPPLER_HZ:.1f} Hz by more than one bin "
        f"({acq.doppler_res_hz:.1f} Hz)"
    )

    # code_phase is a correlation *lag*; the code's actual phase is the
    # inverted quantity -- see the full-chain example's finding #2.
    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    s0 = hitpos + frame

    # --- chip-level zoom: raw signal vs. truth vs. acq's own phase ---------
    n1 = 400
    samp = np.arange(s0, s0 + n1)
    ridx = samp - int(PRE_SILENCE)  # index relative to sig's own origin
    derot = np.exp(-2j * np.pi * (DOPPLER_HZ / FS) * ridx)
    raw_derot = (x[s0 : s0 + n1] * derot).real

    true_chips = data[si[ridx]] * _CSIGN[cph[ridx]]

    acq_local_phase = (chip_phase + (samp - s0) / SPC) % SF
    acq_code = _CSIGN[np.floor(acq_local_phase).astype(int) % SF]
    acq_chips = acq_code * data[si[ridx]]  # acq reports code phase only
    assert np.array_equal(acq_chips, true_chips), (
        "acquisition-reported chip phase disagrees with the true chip "
        "sequence at the hit -- should be exact"
    )

    # --- Monte-Carlo: test_stat / code-phase-error / Doppler-error vs. epoch,
    # over random data and random code phase --------------------------------
    trials = [_mc_trial(t) for t in range(N_TRIALS_MC)]
    ts_mat = np.array([t[0] for t in trials])
    ce_mat = np.array([t[1] for t in trials])
    de_mat = np.array([t[2] for t in trials])
    threshold = acq.threshold
    doppler_bins_mc = acq.doppler_bins
    miss_rate = float(np.mean(np.isnan(ts_mat)))
    ts_min = np.nanmin(ts_mat, axis=0)
    ts_mean = np.nanmean(ts_mat, axis=0)
    ts_max = np.nanmax(ts_mat, axis=0)
    ce_min = np.nanmin(ce_mat, axis=0)
    ce_mean = np.nanmean(ce_mat, axis=0)
    ce_max = np.nanmax(ce_mat, axis=0)
    de_min = np.nanmin(de_mat, axis=0)
    de_mean = np.nanmean(de_mat, axis=0)
    de_max = np.nanmax(de_mat, axis=0)
    bad_epoch = np.abs(ce_mat) > 5.0  # >5 chips: a gross mislock, not noise
    print(
        f"Monte Carlo ({N_TRIALS_MC} trials x {N_EPOCHS_MC} epochs): "
        f"miss_rate={miss_rate:.4f}, threshold={threshold:.2f}, "
        f"worst test_stat={np.nanmin(ts_mat):.2f}, "
        f"mean test_stat={np.nanmean(ts_mat):.2f}, "
        f"gross code-phase mislocks (>5 chips)={np.mean(bad_epoch):.4f}, "
        f"doppler_bins={doppler_bins_mc} (resolution "
        f"{acq.doppler_res_hz:.0f} Hz -- no fine Doppler estimate at this "
        f"operating point)"
    )
    assert miss_rate < 0.02, (
        f"too many epochs fell below the CFAR gate (miss_rate={miss_rate:.4f})"
    )
    assert np.nanmean(ts_mat) > 5 * threshold, (
        "mean test statistic isn't comfortably above threshold"
    )
    assert np.mean(bad_epoch) < 0.10, (
        "too many epochs show a gross code-phase mislock"
    )
    # Every mislock should trace back to a near-threshold epoch, not a
    # random failure mode -- the CFAR gate crossing alone doesn't guarantee
    # an accurate code-phase estimate near threshold.
    assert np.nanmean(ts_mat[bad_epoch]) < np.nanmean(ts_mat[~bad_epoch]), (
        "gross code-phase mislocks aren't concentrated at low test statistic"
    )

    # --- why: replay one bad epoch with noise removed -----------------------
    # If the mislock is noise-driven, stripping the AWGN should clear it up.
    # It doesn't: the code's own partial-epoch (sub-transition) correlation
    # structure is the actual cause -- see the module docstring.
    bad_trial, bad_epoch_idx = map(int, np.argwhere(bad_epoch)[0])
    noisy_err = float(ce_mat[bad_trial, bad_epoch_idx])
    clean_err = _replay_epoch_noiseless(bad_trial, bad_epoch_idx)
    print(
        f"mislock root cause: trial={bad_trial} epoch={bad_epoch_idx} "
        f"noisy_error={noisy_err:.1f} chips, noiseless_replay_error="
        f"{clean_err:.1f} chips -- identical mislock with zero noise "
        f"proves it's structural (a data-transition splitting the "
        f"coherent window), not noise"
    )
    assert abs(clean_err) > 5.0, (
        "expected the mislock to reproduce with noise removed -- if it "
        "didn't, the mislock really was noise-driven and this docstring "
        "claim would be wrong"
    )
    assert clean_err == noisy_err, (
        "noiseless replay landed on a different wrong phase than the "
        "noisy run -- still structural, but not the same failure mode"
    )

    # --- plot -----------------------------------------------------------
    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    a.plot(
        raw_derot,
        label="raw signal (genie-derotated)",
        lw=1,
        color="#1f77b4",
        alpha=0.8,
    )
    a.plot(true_chips, label="true chip x data", lw=1.4, ls="--", color="k")
    a.plot(
        acq_chips,
        label="acquisition-reported phase",
        lw=1,
        color="#d62728",
        alpha=0.8,
    )
    a.set_title("Chip-level zoom at the hit\n(exact code-phase/Doppler match)")
    a.set_xlabel("ADC sample (spc=2)")
    a.legend(fontsize=7, loc="upper right")
    a.set_ylim(-1.6, 1.6)

    ep = np.arange(N_EPOCHS_MC)
    b.fill_between(
        ep, ts_min, ts_max, color="#1f77b4", alpha=0.25, label="min-max band"
    )
    b.plot(ep, ts_mean, color="#1f77b4", lw=1.3, label="mean")
    b.axhline(
        threshold, color="#d62728", lw=1.2, ls="--", label="CFAR threshold"
    )
    b.set_title(
        f"Test statistic vs. epoch\n({N_TRIALS_MC} trials, random data + "
        f"code phase, miss_rate={miss_rate:.3f})",
        fontsize=9,
    )
    b.set_xlabel("code epoch")
    b.set_ylabel("test statistic")
    b.legend(fontsize=7)

    c.fill_between(
        ep, ce_min, ce_max, color="#2ca02c", alpha=0.25, label="min-max band"
    )
    c.plot(ep, ce_mean, color="#2ca02c", lw=1.3, label="mean")
    c.axhline(0, color="k", lw=0.7, ls="--")
    c.set_title(
        f"Code-phase error vs. epoch\n(mislocks >5 chips: "
        f"{np.mean(bad_epoch):.1%} -- coincide with dips above)",
        fontsize=8,
    )
    c.set_xlabel("code epoch")
    c.set_ylabel("error (chips)")
    c.legend(fontsize=7)

    d.fill_between(
        ep, de_min, de_max, color="#9467bd", alpha=0.25, label="min-max band"
    )
    d.plot(ep, de_mean, color="#9467bd", lw=1.3, label="mean")
    d.axhline(0, color="k", lw=0.7, ls="--", label="true Doppler")
    d.set_title(
        f"Doppler error vs. epoch\n(doppler_bins={doppler_bins_mc}, "
        f"res={acq.doppler_res_hz / 1e3:.1f} kHz -- no fine estimate here)",
        fontsize=8,
    )
    d.set_xlabel("code epoch")
    d.set_ylabel("error (Hz)")
    d.legend(fontsize=7)

    fig.suptitle(
        f"Acquisition alone -- CCSDS Gold code (SF={SF}), continuous "
        f"asynchronous BPSK data ({SYM_RATE:.0f} sym/s, "
        f"{TSYM / TE:.3f} epochs/symbol)",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")

    # --- Pd vs. integration depth: coherent (one D-epoch dump) vs. ---------
    # non-coherent (D independent 1-epoch looks, power-summed), same total
    # epochs either way, at a realistic (not deliberately strong) margin.
    div_path = out_path.replace(".png", "_diversity.png")
    coh_results = [
        _diversity_sweep(DIVERSITY_CN0_DBHZ, d, 1, f"COH-{d}") for d in DEPTHS
    ]
    noncoh_results = [
        _diversity_sweep(DIVERSITY_CN0_DBHZ, 1, d, f"NONCOH-{d}")
        for d in DEPTHS
    ]
    coh_pd = np.array([r[6] for r in coh_results])
    noncoh_pd = np.array([r[6] for r in noncoh_results])

    assert coh_pd[0] < 0.2 and noncoh_pd[0] < 0.2, (
        f"expected the 1-epoch depth to be far short of pd=0.9 here "
        f"(coh={coh_pd[0]:.3f}, noncoh={noncoh_pd[0]:.3f})"
    )
    assert noncoh_pd[-1] > 0.85, (
        f"expected non-coherent to reach close to its own pd=0.9 target "
        f"at the deepest swept depth (noncoh={noncoh_pd[-1]:.3f})"
    )
    # The real, more interesting finding: coherent does NOT reach its own
    # theoretical target here, because an 11-epoch coherent window spans
    # ~8 symbols (~7 data transitions) -- the same self-cancellation
    # mechanism from the mislock story above now costs real Pd, not just
    # occasional mislocks, while non-coherent (blind to phase between
    # looks) is immune to it and comfortably clears the target.
    assert coh_pd[-1] < noncoh_pd[-1] - 0.1, (
        "expected coherent combining to meaningfully underperform "
        f"non-coherent at the deepest depth (coh={coh_pd[-1]:.3f}, "
        f"noncoh={noncoh_pd[-1]:.3f})"
    )
    # Binomial standard error per point, for the error bars below.
    coh_n = np.array(
        [N_TRIALS_MC * (r[0].shape[1] // r[4]) for r in coh_results]
    )
    noncoh_n = np.array(
        [N_TRIALS_MC * (r[0].shape[1] // r[4]) for r in noncoh_results]
    )
    coh_se = np.sqrt(coh_pd * (1 - coh_pd) / coh_n)
    noncoh_se = np.sqrt(noncoh_pd * (1 - noncoh_pd) / noncoh_n)
    pd_predicted = [r[7] for r in coh_results]  # same at every depth

    # --- theoretical Pd with a uniformly-distributed transition ------------
    # (semi-analytical: exact chip-timing combinatorics, no noise Monte
    # Carlo -- see _theoretical_pd's docstring).
    snr_th = np.sqrt(10.0 ** (DIVERSITY_CN0_DBHZ / 10.0) / FS)
    coh_theory = np.array(
        [
            _theoretical_pd(d, snr_th, SF * SPC, r[8], 1)
            for d, r in zip(DEPTHS, coh_results)
        ]
    )
    noncoh_theory = np.array(
        [
            _theoretical_pd(d, snr_th, SF * SPC, r[8], r[4])
            for d, r in zip(DEPTHS, noncoh_results)
        ]
    )
    noncoh_theory_err = float(np.max(np.abs(noncoh_theory - noncoh_pd)))
    coh_theory_err = float(np.max(np.abs(coh_theory - coh_pd)))
    print(
        f"theory vs. empirical: non-coherent max|diff|="
        f"{noncoh_theory_err:.4f}, coherent max|diff|={coh_theory_err:.4f}"
    )
    assert noncoh_theory_err < 0.05, (
        "expected the independent-look non-coherent theory (exact "
        "chip-timing combinatorics through marcum_q, no noise Monte "
        f"Carlo) to closely match the empirical sweep (max diff "
        f"{noncoh_theory_err:.4f})"
    )
    # Coherent theory is deliberately a partial model (the window's own
    # D-point DFT peak, capturing Doppler-bin leakage from a mid-window
    # phase step) -- it should track empirical Pd's rise-then-plateau
    # shape and stay close to or below it (the code-phase axis has an
    # analogous leakage effect this model doesn't include, which is why
    # real Pd ends up higher at most depths -- see the gallery page). A
    # small overshoot at low D (finite-sample noise on the empirical
    # side) is expected; a large one would mean the DFT-peak model is
    # over-crediting.
    assert np.all(coh_theory <= coh_pd + 0.1), (
        "coherent theory overshot empirical Pd by more than expected "
        "finite-sample noise -- the DFT-peak model may be over-crediting"
    )

    fig2, ax2 = plt.subplots(figsize=(8, 5.5))
    ax2.errorbar(
        DEPTHS,
        coh_pd,
        yerr=1.96 * coh_se,
        color="#1f77b4",
        marker="o",
        ms=4,
        lw=1.4,
        capsize=3,
        label="coherent (1 dump, D epochs)",
    )
    ax2.errorbar(
        DEPTHS,
        noncoh_pd,
        yerr=1.96 * noncoh_se,
        color="#d62728",
        marker="s",
        ms=4,
        lw=1.4,
        capsize=3,
        label="non-coherent (D looks)",
    )
    ax2.plot(
        DEPTHS,
        coh_theory,
        color="#1f77b4",
        lw=1.0,
        ls="--",
        alpha=0.7,
        label="coherent theory (uniform transition)",
    )
    ax2.plot(
        DEPTHS,
        noncoh_theory,
        color="#d62728",
        lw=1.0,
        ls="--",
        alpha=0.7,
        label="non-coherent theory (uniform transition)",
    )
    ax2.axhline(0.9, color="k", lw=0.8, ls="--", label="target pd = 0.9")
    ax2.set_ylim(0, 1.0)
    ax2.set_xlabel("integration depth D (code epochs)")
    ax2.set_ylabel("empirical Pd")
    ax2.set_title(
        f"Detection probability vs. integration depth\n"
        f"Es/N0={DIVERSITY_ES_N0_DB:.0f} dB "
        f"(C/N0={DIVERSITY_CN0_DBHZ:.1f} dB-Hz), {N_TRIALS_MC} trials/depth, "
        f"95% CI"
    )
    ax2.legend(fontsize=9, loc="lower right")
    fig2.tight_layout()
    fig2.savefig(div_path, dpi=120)
    print(f"wrote {div_path} (pd_predicted={pd_predicted[-1]:.3f})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dsss_acq_async_data_demo.png")
