"""dsss_receiver_stress.py -- randomized stress test for the composed
`DsssReceiver` C object across C/N0, Doppler, front-end sample rate, signal
amplitude, and data sequence, instead of the single fixed operating point
(CN0=97 dB-Hz, Doppler=50 Hz, one seed) every other `DsssReceiver` test
exercises.

**Signal generation goes through `wfmgen`, not hand-rolled numpy trig.**
`type="dsss"` alone cannot produce this story's continuous, asynchronous-
data signal (it is burst-framed, and its `data_code` length fixes an exact
integer chips-per-symbol ratio -- `3e6/2100 = 1428.57` isn't one). Instead,
compose three `Synth` calls at a shared generation rate (``FS_GEN``, the
exact LCM of `chip_rate` and `symbol_rate`, so both the code's and the
data's oversample factors are exact integers while their *ratio* stays
non-integer -- the asynchronous phase walk this story needs):

  1. ``Synth(type="bits", bits=<Gold code>, sps=CHIP_SPS)`` -- the
     continuously-repeating spreading code (cycled automatically).
  2. ``Synth(type="bits", bits=<random data>, sps=DATA_SPS)`` -- one
     trial's random data bits, held (not cycled -- the full sequence is
     supplied).
  3. Elementwise-multiply the two (plain array multiply, the only
     hand-written "DSP") -> the spread baseband, then
     ``Synth(type="symbols", symbols=..., freq=doppler_hz, snr=cn0_dbhz,
     snr_mode="fs")`` for wfmgen's native Doppler mixing + AWGN injection.

The result is resampled (`doppler.resample.RateConverter`) down to each
trial's target front-end rate -- this realizes the "sample rate" axis (a
real SDR captures at whatever rate its front end happens to run) and
connects the wfmgen generation rate to a realistic receiver input rate in
one step.

Two real findings surfaced while building this (kept here, not smoothed
over -- see the module for what each means for interpreting results):

* **Doppler pull-in is narrow.** At CN0=65 dB-Hz, Doppler=50 Hz decodes at
  BER=0.0000; Doppler=500-1000 Hz decodes at chance level (BER~0.41), with
  `rx.doppler_hz` stuck at 0.0 regardless of the true value -- reproduced
  identically through the pure-numpy generator used by every other
  `DsssReceiver` test, so it is a real receiver characteristic (Acquisition
  defers Doppler resolution to `MpskReceiver`'s own carrier loop, whose
  pull-in range is evidently much narrower than this story's intended
  operating envelope), not a signal-generation artifact. `find_doppler_
  pullin_boundary` below characterizes it directly instead of guessing.
* **wfmgen's Doppler mixing has a small, growing long-run phase error**
  (a few mrad by 30M samples -- the signature of a slightly-quantized
  frequency word), which only matters for very long runs: at Doppler=50 Hz
  it stays clean through ~1000-2000 symbols and fails by 3500. `N_SYM`
  below is chosen inside the verified-clean window; this is a separate,
  flagged, deferred finding, not fixed here.

Run::

    python -m doppler.examples.dsss_receiver_stress
"""

from __future__ import annotations

import math
import sys
import warnings

import numpy as np

from doppler.dsss import DsssReceiver
from doppler.resample import RateConverter
from doppler.snr import snr_data_aided_db
from doppler.wfm import Gold, Synth

# ── link geometry (fixed) ────────────────────────────────────────────────
SF = 1023  # CCSDS Gold code period
CHIP_RATE = 3.0e6
SYM_RATE = 2100.0
SPAN_HZ = CHIP_RATE / (2 * SF)  # native Doppler search half-range (~1466.3 Hz)

# Generation rate: the exact LCM of chip_rate and symbol_rate, so both the
# code's and the data's oversample factors are integers while their ratio
# (the asynchronous chips-per-symbol walk) stays non-integer.
FS_GEN = float(math.lcm(int(CHIP_RATE), int(SYM_RATE)))  # 21e6
CHIP_SPS = int(FS_GEN / CHIP_RATE)  # 7
DATA_SPS = int(FS_GEN / SYM_RATE)  # 10000

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)

# Randomized axes.
CN0_RANGE_DBHZ = (40.0, 80.0)
DOPPLER_FRAC_OF_SPAN = 0.9  # keep drawn Doppler safely inside SPAN_HZ
SPC_CHOICES = (2, 3, 4, 5, 6, 8)
POWER_SCALE_LOG10_RANGE = (-2.0, 2.0)  # 0.01x - 100x amplitude

N_SYM = 700  # inside the verified-clean window for the long-run drift above


def make_signal_wfmgen(cn0_dbhz, doppler_hz, seed, n_sym=N_SYM):
    """Build one trial's continuous, asynchronous DSSS signal at ``FS_GEN``
    entirely through ``wfmgen`` primitives (see module docstring for the
    composition). Returns ``(x, data_bits)`` -- complex64 samples at
    ``FS_GEN`` and the known 0/1 transmitted bits."""
    rng = np.random.default_rng(seed)
    data_bits = rng.integers(0, 2, n_sym).astype(np.uint8)
    n_total = n_sym * DATA_SPS

    chip_stream = Synth(
        type="bits",
        bits=CODE.tobytes(),
        modulation="bpsk",
        sps=CHIP_SPS,
        fs=FS_GEN,
    ).steps(n_total)
    data_stream = Synth(
        type="bits",
        bits=data_bits.tobytes(),
        modulation="bpsk",
        sps=DATA_SPS,
        fs=FS_GEN,
    ).steps(n_total)
    composite = (chip_stream * data_stream).astype(np.complex64)

    out = Synth(
        type="symbols",
        symbols=composite,
        sps=1,
        freq=doppler_hz,
        snr=cn0_dbhz,
        snr_mode="fs",
        seed=seed,
        fs=FS_GEN,
    ).steps(n_total)
    return out, data_bits


def _lag_search_ber(bits, data_bits, max_lag=50):
    """Best-of-both-polarities BER over a lag search, matching the
    convention already used by dsss_receiver_demo.py/test_dsss_receiver.py."""
    truth = np.where(data_bits > 0, -1.0, 1.0)
    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    best_ber, best_lag, best_inv = 1.0, 0, False
    for lag in range(-max_lag, max_lag + 1):
        ti = lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(truth):
            continue
        e_same = float(np.mean(bits[lo:hi] != truth[ti]))
        e_inv = float(np.mean(bits[lo:hi] != -truth[ti]))
        if e_same < best_ber:
            best_ber, best_lag, best_inv = e_same, lag, False
        if e_inv < best_ber:
            best_ber, best_lag, best_inv = e_inv, lag, True
    return best_ber, best_lag, best_inv


def run_trial(
    seed,
    cn0_dbhz=None,
    doppler_hz=None,
    spc=None,
    power_scale=None,
    n_sym=N_SYM,
):
    """Run one randomized trial (or a pinned one, for the pull-in
    bisection below) and return a full diagnostic record -- ground truth,
    every DsssReceiver property, measured Es/N0, and BER. Nothing here is
    silently discarded: this harness's job is surfacing bugs with a fully
    reproducible parameter tuple, not just gating on a pass rate."""
    rng = np.random.default_rng(seed)
    if cn0_dbhz is None:
        cn0_dbhz = rng.uniform(*CN0_RANGE_DBHZ)
    if doppler_hz is None:
        doppler_hz = rng.uniform(
            -DOPPLER_FRAC_OF_SPAN * SPAN_HZ, DOPPLER_FRAC_OF_SPAN * SPAN_HZ
        )
    if spc is None:
        spc = int(rng.choice(SPC_CHOICES))
    if power_scale is None:
        power_scale = 10.0 ** rng.uniform(*POWER_SCALE_LOG10_RANGE)

    x_gen, data_bits = make_signal_wfmgen(cn0_dbhz, doppler_hz, seed, n_sym)

    fs_front = CHIP_RATE * spc
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)
    x = (x * power_scale).astype(np.complex64)

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        rx = DsssReceiver(
            CODE,
            chip_rate=CHIP_RATE,
            symbol_rate=SYM_RATE,
            spc=spc,
            cn0_dbhz=max(cn0_dbhz, 30.0),
            doppler_uncertainty=0.95 * SPAN_HZ,
            reps=16,
            max_noncoh=8,
            segments=4,
            sps=8,
        )

    te = SF * spc
    syms_parts = []
    for pos in range(0, len(x) - te, te):
        out = rx.steps(x[pos : pos + te])
        if len(out):
            syms_parts.append(out)
    syms = (
        np.concatenate(syms_parts) if syms_parts else np.zeros(0, np.complex64)
    )

    record = {
        "seed": seed,
        "cn0_true_dbhz": cn0_dbhz,
        "doppler_true_hz": doppler_hz,
        "spc": spc,
        "power_scale": power_scale,
        "n_syms_recovered": len(syms),
        "tracking": rx.tracking,
        "doppler_hz": rx.doppler_hz,
        "cn0_dbhz_est": rx.cn0_dbhz_est,
        "chip_phase": rx.chip_phase,
        "code_rate": rx.code_rate,
        "lock": rx.lock,
        "norm_freq": rx.norm_freq,
        "segments": rx.segments,
        "sps": rx.sps,
        "n": rx.n,
    }

    if len(syms) < 10:
        record.update(
            ber=1.0, lag=0, inverted=False, esn0_meas_db=float("nan")
        )
        return record

    bits = np.where(syms.real > 0, 1.0, -1.0)
    ber, lag, inverted = _lag_search_ber(bits, data_bits)

    # Measured Es/N0 (data-aided) against the known bits at the winning
    # lag/polarity -- an independent diagnostic from BER (see module doc).
    # syms[k] corresponds to data_bits[k + lag] over the overlapping range.
    k0, k1 = max(0, -lag), min(len(syms), len(data_bits) - lag)
    if k1 > k0:
        soft = syms[k0:k1] if not inverted else -syms[k0:k1]
        sign_bits = data_bits[k0 + lag : k1 + lag]
        esn0_meas = snr_data_aided_db(soft.astype(np.complex64), sign_bits)
    else:
        esn0_meas = float("nan")

    record.update(ber=ber, lag=lag, inverted=inverted, esn0_meas_db=esn0_meas)
    return record


def find_doppler_pullin_boundary(
    cn0_dbhz=65.0, seed=1000, n_sym=N_SYM, tol_hz=10.0
):
    """Bisect |Doppler| between 0 and the near-DC clean point and the
    engine's search span to find the transition from clean decode
    (BER<0.02) to chance-level (BER>0.3), holding C/N0/spc/power fixed.
    Returns the boundary in Hz (the largest |Doppler| still decoding
    cleanly), or None if even Doppler=0 fails (a much larger problem)."""
    lo_hz, hi_hz = 0.0, DOPPLER_FRAC_OF_SPAN * SPAN_HZ

    r0 = run_trial(
        seed,
        cn0_dbhz=cn0_dbhz,
        doppler_hz=lo_hz,
        spc=2,
        power_scale=1.0,
        n_sym=n_sym,
    )
    if r0["ber"] > 0.3:
        return None  # doesn't even work at Doppler=0 -- a different problem

    r1 = run_trial(
        seed,
        cn0_dbhz=cn0_dbhz,
        doppler_hz=hi_hz,
        spc=2,
        power_scale=1.0,
        n_sym=n_sym,
    )
    if r1["ber"] < 0.02:
        return hi_hz  # works across the whole searchable range

    while hi_hz - lo_hz > tol_hz:
        mid = 0.5 * (lo_hz + hi_hz)
        r = run_trial(
            seed,
            cn0_dbhz=cn0_dbhz,
            doppler_hz=mid,
            spc=2,
            power_scale=1.0,
            n_sym=n_sym,
        )
        if r["ber"] < 0.02:
            lo_hz = mid
        else:
            hi_hz = mid
    return lo_hz


def sweep_cn0_near_zero_doppler(n_per_bucket=8, doppler_hz=20.0, seed0=5000):
    """Dedicated C/N0 sweep at a fixed, comfortably-inside-pull-in Doppler,
    nominal front-end rate, and nominal amplitude -- deliberately NOT drawn
    from the random full sweep, for two reasons: (1) the near-zero-Doppler
    band is only ~7% of the searchable Doppler range, so a random draw
    rarely lands enough trials there (observed: 0 near-zero trials in the
    weakest C/N0 bucket out of 30 random draws) to make a bucketed C/N0-
    monotonicity check statistically meaningful; (2) pinning ``spc=2``/
    ``power_scale=1.0`` isolates the C/N0 axis cleanly -- an earlier draft
    left those random too and the resulting "C/N0 doesn't look monotonic"
    signal turned out to be the (real, but separate) power-sensitivity
    axis leaking in (a low-power_scale trial failing had nothing to do
    with its C/N0 bucket). The random full sweep below is where power/spc
    sensitivity gets exercised instead. Returns one clean-decode rate per
    C/N0 bucket."""
    edges = np.linspace(*CN0_RANGE_DBHZ, 4)
    rates = []
    for i, (lo, hi) in enumerate(zip(edges[:-1], edges[1:])):
        rng = np.random.default_rng(seed0 + i)
        cn0s = rng.uniform(lo, hi, n_per_bucket)
        recs = [
            run_trial(
                seed0 + i * 1000 + k,
                cn0_dbhz=c,
                doppler_hz=doppler_hz,
                spc=2,
                power_scale=1.0,
            )
            for k, c in enumerate(cn0s)
        ]
        clean = [r for r in recs if r["ber"] < 0.02]
        rates.append(len(clean) / len(recs))
        print(
            f"  CN0 in [{lo:.0f},{hi:.0f}) dB-Hz, Doppler={doppler_hz:.0f}Hz: "
            f"{len(clean)}/{len(recs)} clean ({rates[-1]:.0%})"
        )
    return rates


def main(n_trials=300, out_path="dsss_receiver_stress.png"):
    rng = np.random.default_rng(0)
    records = [run_trial(int(s)) for s in rng.integers(0, 2**31 - 1, n_trials)]

    boundary_hz = find_doppler_pullin_boundary()
    boundary_str = (
        "FAILED AT DOPPLER=0"
        if boundary_hz is None
        else f"{boundary_hz:.0f} Hz"
    )
    print(
        f"Doppler pull-in boundary (CN0=65 dB-Hz): {boundary_str} "
        f"(native search span is +/-{SPAN_HZ:.0f} Hz)"
    )

    print(
        "\nC/N0 monotonicity check (dedicated sweep, fixed near-zero Doppler):"
    )
    bucket_rates = sweep_cn0_near_zero_doppler()

    # Full BER-vs-|Doppler| picture, all trials (diagnostic, not asserted).
    print("\nBER vs |Doppler| (all trials, all C/N0):")
    dopp_edges = np.linspace(0, DOPPLER_FRAC_OF_SPAN * SPAN_HZ, 6)
    for lo, hi in zip(dopp_edges[:-1], dopp_edges[1:]):
        in_bucket = [
            r for r in records if lo <= abs(r["doppler_true_hz"]) < hi
        ]
        clean = [r for r in in_bucket if r["ber"] < 0.02]
        rate = len(clean) / len(in_bucket) if in_bucket else float("nan")
        print(
            f"  |Doppler| in [{lo:.0f},{hi:.0f}) Hz: "
            f"{len(clean)}/{len(in_bucket)} clean ({rate:.0%})"
            if in_bucket
            else f"  |Doppler| in [{lo:.0f},{hi:.0f}) Hz: no trials drawn"
        )

    # A trial well *inside* the measured pull-in boundary that still
    # decoded at chance level is a genuine anomaly (outside the boundary,
    # chance-level decode is the expected, already-mapped pull-in limit --
    # a margin below the boundary itself avoids flagging trials that are
    # just close to the (noisy, single-CN0-point) measured edge).
    safe_hz = 0.7 * boundary_hz if boundary_hz else 0.0
    anomalies = [
        r
        for r in records
        if r["tracking"] == 1
        and r["ber"] > 0.3
        and abs(r["doppler_true_hz"]) < safe_hz
    ]
    if anomalies:
        print(
            f"\n{len(anomalies)} ANOMALOUS trial(s) -- locked but decoded "
            f"at chance level well inside the pull-in boundary "
            f"(< {safe_hz:.0f} Hz):"
        )
        for r in anomalies:
            print(f"  {r}")

    valid_rates = [r for r in bucket_rates if not math.isnan(r)]
    assert valid_rates == sorted(valid_rates), (
        "clean-decode rate should be non-decreasing with C/N0 within the "
        f"near-zero-Doppler bucket, got {bucket_rates}"
    )
    if valid_rates:
        assert valid_rates[-1] >= 0.90, (
            f"expected >=90% clean decode in the strongest C/N0 bucket "
            f"(|Doppler|<100Hz), got {valid_rates[-1]:.0%}"
        )
    assert boundary_hz is not None and boundary_hz > 20.0, (
        f"Doppler pull-in boundary regressed to near-zero: {boundary_hz}"
    )

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, ax = plt.subplots(figsize=(7, 5))
        dopp = np.array([abs(r["doppler_true_hz"]) for r in records])
        ber = np.array([r["ber"] for r in records])
        cn0 = np.array([r["cn0_true_dbhz"] for r in records])
        sc = ax.scatter(dopp, ber, c=cn0, cmap="viridis", s=14, alpha=0.7)
        ax.axvline(
            boundary_hz,
            color="red",
            ls="--",
            lw=1,
            label=f"pull-in boundary ({boundary_hz:.0f} Hz)",
        )
        ax.set_xlabel("|Doppler| (Hz)")
        ax.set_ylabel("BER")
        ax.set_title(
            f"DsssReceiver stress sweep ({n_trials} random trials)\n"
            "color = true C/N0 (dB-Hz)"
        )
        ax.legend(fontsize=8)
        fig.colorbar(sc, ax=ax, label="C/N0 (dB-Hz)")
        fig.tight_layout()
        fig.savefig(out_path, dpi=110)
        print(f"\nwrote {out_path}")
    except ImportError:
        pass


if __name__ == "__main__":
    main(
        out_path=sys.argv[1]
        if len(sys.argv) > 1
        else "dsss_receiver_stress.png"
    )
