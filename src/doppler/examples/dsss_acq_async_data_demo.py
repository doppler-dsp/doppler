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
2. **Test statistic vs. epoch, Monte-Carlo over random data and code
   phase.** ``doppler_bins=1`` at this operating point, so each ``push()``
   evaluates exactly one code epoch -- a direct per-epoch window onto
   how asynchronous data modulation affects the search.
3. **Code-phase error vs. epoch, same sweep.** Near-zero almost
   everywhere, with occasional gross (hundreds-of-chips) mislocks at the
   same epochs where the test statistic dipped.
4. **Doppler error vs. epoch.** Flat at the full injected offset --
   ``doppler_bins=1`` means this operating point makes no attempt at fine
   Doppler resolution at all (that's a downstream tracking-loop job).

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

Downstream despread/demod (``Dll(segments) -> MpskReceiver``) is a
separate, later stage of this story -- this page is deliberately
acquisition-only.

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

# Deliberately strong (not a sensitivity study) to unambiguously validate
# the search mechanics -- see the module docstring. It reads as a high
# dB-Hz figure because C/N0 is normalised by the front-end sample rate
# (FS=6 MHz here): a fixed C/N0 buys much less per-raw-sample SNR at a
# high sample rate than at a narrowband one.
CN0_OPERATING_DBHZ = 97.0
SEED = 6

N_EPOCHS_MC = 100  # Monte-Carlo observation window, in code epochs
N_TRIALS_MC = 200  # independent random-data/random-code-phase trials


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
    random-data/random-phase trial.

    Builds ``N_EPOCHS_MC`` epochs of continuous signal (no silence --
    this experiment is about the search's per-epoch behaviour once
    already in steady transmission, not acquisition latency), random
    data and a random starting code phase, and streams it through a
    fresh ``Acquisition`` instance one epoch (one ``push()``) at a time.
    Returns three length-``N_EPOCHS_MC`` arrays (test statistic, code-phase
    error in chips, Doppler error in Hz), NaN where that epoch's peak fell
    below the CFAR gate (a real miss, not a bug).
    """
    rng = np.random.default_rng(1000 + trial)
    n = N_EPOCHS_MC * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, int(n / TSYM) + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    phase0 = int(rng.integers(0, SF))
    cph = ((idx / SPC).astype(int) + phase0) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)
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
    dopp_err = np.full(nframes, np.nan)  # Hz, vs. true DOPPLER_HZ
    pos = 0
    for i in range(nframes):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            dop_bin, code_phase, _pk, _n, test_stat, _c = hits[0]
            ts[i] = test_stat
            chip_phase = (SF - code_phase / SPC) % SF
            code_err[i] = ((chip_phase - phase0 + SF / 2) % SF) - SF / 2
            k_fold = (
                dop_bin + doppler_bins // 2
            ) % doppler_bins - doppler_bins // 2
            dopp_err[i] = k_fold * doppler_res_hz - DOPPLER_HZ
        pos += frame
    return ts, code_err, dopp_err


def _replay_epoch_noiseless(trial: int, epoch: int) -> float:
    """Rebuild one Monte-Carlo (trial, epoch) with all injected noise
    removed and report its code-phase error in chips.

    Uses the exact same ``rng``/``phase0`` construction as ``_mc_trial``
    for that trial, so this reproduces the identical code phase, data
    sequence, and data-bit-transition timing -- only the AWGN term is
    dropped. If the mislock survives noiseless, it is a deterministic
    property of the code and the transition's position within the epoch,
    not a noise event.
    """
    rng = np.random.default_rng(1000 + trial)
    n = N_EPOCHS_MC * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, int(n / TSYM) + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    phase0 = int(rng.integers(0, SF))
    cph = ((idx / SPC).astype(int) + phase0) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)
    e0, e1 = epoch * TE, (epoch + 1) * TE
    ep_clean = sig[e0:e1].astype(np.complex64)  # no noise added

    acq = _new_acq()
    hits = acq.push(ep_clean)
    assert hits, "noiseless epoch produced no detection at all"
    _dop_bin, code_phase, _pk, _n, _ts, _c = hits[0]
    chip_phase = (SF - code_phase / SPC) % SF
    return float(((chip_phase - phase0 + SF / 2) % SF) - SF / 2)


# --8<-- [start:diversity_configs]
# Same total energy budget across the last three (2-3 epochs each); only how
# it's combined differs. cn0_dbhz is a sizing target, not the real injected
# signal (CN0_OPERATING_DBHZ, always 97 dB-Hz) -- lowering it is how each
# config is pushed past the single-epoch coherent ceiling.
N_EPOCHS_DIV = 99  # divisible by 2 and 3, for the epoch-diversity comparison
DIVERSITY_CONFIGS = [
    (55.0, 1, 1, "1 epoch/decision (coherent, baseline)"),
    (50.0, 3, 1, "2 epochs/decision (coherent)"),
    (49.0, 3, 1, "3 epochs/decision (coherent, 1 dump)"),
    (48.0, 1, 4, "3 epochs/decision (non-coherent, 3 looks)"),
]
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
    """
    rng = np.random.default_rng(1000 + trial)
    n = N_EPOCHS_DIV * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, int(n / TSYM) + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    phase0 = int(rng.integers(0, SF))
    cph = ((idx / SPC).astype(int) + phase0) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)
    amp_snr = np.sqrt(10.0 ** (CN0_OPERATING_DBHZ / 10.0) / FS)
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
            _dop, code_phase, _pk, _n, test_stat, _c = hits[0]
            ts[i] = test_stat
            chip_phase = (SF - code_phase / SPC) % SF
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
    thr = acq_ref.eta_nc if acq_ref.n_noncoh > 1 else acq_ref.threshold
    print(
        f"epoch-diversity[{label}]: doppler_bins={acq_ref.doppler_bins} "
        f"n_noncoh={acq_ref.n_noncoh} threshold={thr:.2f} "
        f"mislock_rate={mislock_rate:.4f} "
        f"({int(np.sum(mislock))} of {int(np.sum(valid))})"
    )
    return (
        ts_mat,
        ce_mat,
        thr,
        acq_ref.doppler_bins,
        acq_ref.n_noncoh,
        mislock_rate,
    )


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
    dop_bin, code_phase, _peak_mag, _noise_est, test_stat, cn0_dbhz_est = hit
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
    chip_phase = (SF - code_phase / SPC) % SF
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

    # --- epoch-diversity comparison: does *any* combining across >=2 ------
    # independent epochs (coherent or non-coherent) fix the mislock, and
    # does non-coherent still win on variance even where both reach zero?
    div_path = out_path.replace(".png", "_diversity.png")
    results = [
        _diversity_sweep(cn0, reps, mnc, label)
        for cn0, reps, mnc, label in DIVERSITY_CONFIGS
    ]
    baseline_rate = results[0][5]
    assert baseline_rate > 0.01, (
        "expected the 1-epoch baseline to show a real mislock rate here"
    )
    for result, (_cn0, _reps, _mnc, label) in zip(
        results[1:], DIVERSITY_CONFIGS[1:]
    ):
        rate = result[5]
        assert rate < 0.005, (
            f"expected epoch diversity to clear the mislock for {label!r} "
            f"(mislock_rate={rate:.4f})"
        )

    # Shared y-scale across all four panels -- otherwise each config's much
    # larger coherent-gain peak (more epochs combined) visually shrinks its
    # own band, making the mislock-vs-margin comparison misleading. Capped
    # (not the data max, ~160 for the 3-epoch coherent dump): the mislock
    # margin against threshold lives well under 100, and the multi-epoch
    # configs' peaks are already unambiguously off the top of the chart.
    shared_ymax = 100.0

    fig2, axs2 = plt.subplots(2, 2, figsize=(11, 8.5))
    for ax, result, config in zip(axs2.flat, results, DIVERSITY_CONFIGS):
        ts_mat, _ce_mat, thr, db, nnc, mislock_rate = result
        label = config[3]
        ep2 = np.arange(ts_mat.shape[1]) * db
        valid = ~np.all(np.isnan(ts_mat), axis=0)
        ep_v, ts_v = ep2[valid], ts_mat[:, valid]
        ts_min, ts_mean, ts_max = (
            np.nanmin(ts_v, axis=0),
            np.nanmean(ts_v, axis=0),
            np.nanmax(ts_v, axis=0),
        )
        if nnc > 1:
            ax.vlines(ep_v, ts_min, ts_max, color="#1f77b4", alpha=0.4, lw=1.5)
            ax.plot(ep_v, ts_mean, "o-", color="#1f77b4", lw=1.2, ms=3)
        else:
            ax.fill_between(ep_v, ts_min, ts_max, color="#1f77b4", alpha=0.25)
            ax.plot(ep_v, ts_mean, color="#1f77b4", lw=1.2)
        ax.axhline(thr, color="#d62728", lw=1.2, ls="--")
        ax.set_ylim(0, shared_ymax)
        ax.set_title(f"{label}\nmislock_rate={mislock_rate:.4f}", fontsize=9)
        ax.set_xlabel("code epoch (real time)")
        ax.set_ylabel("test statistic")

    fig2.suptitle(
        "Epoch-diversity comparison: coherent vs. non-coherent combining "
        f"across independent epochs ({N_TRIALS_MC} trials each)",
        fontsize=11,
    )
    fig2.tight_layout(rect=(0, 0, 1, 0.95))
    fig2.subplots_adjust(hspace=0.4, wspace=0.3)
    fig2.savefig(div_path, dpi=120)
    print(f"wrote {div_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dsss_acq_async_data_demo.png")
