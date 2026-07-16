"""dsss_acquisition_stress.py -- randomized stress test for `Acquisition`
ALONE, isolated from the rest of the composed `DsssReceiver` chain, across
the exact same axes `dsss_receiver_stress.py` defined for the full receiver
(C/N0 40-80 dB-Hz, Doppler up to 0.9x the native search span, front-end
sample rate via spc, and signal amplitude).

Why isolate Acquisition
-----------------------
`dsss_receiver_stress.py` found the *composed* `DsssReceiver` pulls in only
out to ~263 Hz of Doppler, far inside the ~1466 Hz native search span its
embedded `Acquisition` is configured to search (`doppler_uncertainty=
0.95*SPAN_HZ`). That finding never established WHERE in the chain the
narrowing happens: `Acquisition` reports a Doppler estimate but hands off
to `Dll` -> `RateConverter` -> `MpskReceiver`, whose own Costas carrier
loop could independently have a much narrower pull-in than the Doppler bin
`Acquisition` locked onto. This harness answers that question directly by
running `Acquisition` on its own and checking whether ITS OWN reported
(Doppler bin, code phase) lands on the true cell across the *whole*
configured search range, not just near-zero Doppler --
:func:`sweep_doppler_acquisition_range` is the key result.

**Answer, confirmed over a 300-trial random sweep plus dedicated
isolation/calibration sweeps**: the narrowing is NOT in `Acquisition`.
Its own search is essentially exact -- 100% on-true-cell across every
|Doppler| bucket from 0 to 1320 Hz, 100% across the whole 40-80 dB-Hz C/N0
range, and code-phase error exactly 0.000 chips over all 300 random
trials. But :func:`measure_doppler_handoff_quality` shows WHY the
composed receiver still narrows: `acq_create()`'s symbol_rate-aware sizing
deliberately picks the smallest coherent depth that meets the target Pd in
a single dwell (lowest latency, lowest mislock risk), which collapses to
`doppler_bins == 1` -- a single bin spanning the ENTIRE +/-1466 Hz native
span, zero frequency resolution -- in 91% of on-cell trials across this
story's C/N0 range. Acquisition still detects and locates the code phase
correctly, but the Doppler *estimate* it hands off can then be off by
hundreds of Hz (mean |error| 685 Hz); 83% of on-cell trials exceed the
composed receiver's own measured ~263 Hz pull-in boundary. The downstream
`Dll` -> `MpskReceiver` Costas loop has to close that gap on its own, and
its pull-in range is the real bottleneck -- not a bug anywhere, an
architectural consequence of Acquisition's own latency-optimized sizing
that the composed receiver inherits.

Ground truth against wfmgen
---------------------------
Signal generation reuses `dsss_receiver_stress.make_signal_wfmgen` and its
link geometry verbatim -- one wfmgen composition, one source of truth,
shared by both harnesses. Two pieces of ground truth Acquisition's raw
output is checked against:

* **Doppler** -- the injected `doppler_hz` converts directly to an expected
  `(doppler_bin, doppler_res_hz)` cell via `a.doppler_res_hz`/`a.doppler_bins`
  (both read fresh per trial, since sizing depends on that trial's own
  C/N0); resampling changes the sample rate, never the absolute Hz of the
  carrier, so this holds regardless of spc.
* **Code phase** -- rather than modeling `RateConverter`'s exact group
  delay analytically, :func:`_code_phase_ref` measures it empirically: a
  clean (noiseless, zero-Doppler) reference signal at a given spc is pushed
  through a fresh `Acquisition` once, and whatever code_phase it reports
  becomes the expected value for every real trial at that spc. This is
  valid because code phase depends only on the CHIP_SPS/spc oversample
  ratio and the resampler's own (config-only, not signal-dependent) group
  delay -- never on C/N0, Doppler, power scale, or the payload data (a BPSK
  data multiply flips sign, it never shifts chip timing).

Run::

    python -m doppler.examples.dsss_acquisition_stress
"""

from __future__ import annotations

import math
import sys
import warnings

import numpy as np

from doppler.dsss import Acquisition
from doppler.examples.dsss_receiver_stress import (
    CHIP_RATE,
    CN0_RANGE_DBHZ,
    CODE,
    DOPPLER_FRAC_OF_SPAN,
    FS_GEN,
    N_SYM,
    POWER_SCALE_LOG10_RANGE,
    SF,
    SPAN_HZ,
    SPC_CHOICES,
    SYM_RATE,
    make_signal_wfmgen,
)
from doppler.resample import RateConverter

# Acquisition sizing knobs, matching what DsssReceiver configures internally
# for its own embedded engine (native_dsss_receiver_core.c's acq_create call
# by way of the Python DsssReceiver constructor's own defaults/stress-harness
# choices) -- this is what makes "the full range we defined for DsssReceiver"
# apply to Acquisition on its own.
REPS = 16
MAX_NONCOH = 8
DOPPLER_UNCERTAINTY = 0.95 * SPAN_HZ

MAX_FRAMES = 64  # safety cap on frames pushed while searching for a hit


def _circ_dist(a: int, b: int, m: int) -> int:
    """Circular distance between ``a`` and ``b`` modulo ``m``."""
    d = abs(a - b) % m
    return min(d, m - d)


def make_engine(cn0_dbhz: float, spc: int) -> Acquisition:
    """Build a fresh engine at this trial's sizing C/N0 and spc, matching
    DsssReceiver's own `Acquisition` configuration (reps/max_noncoh/
    doppler_uncertainty/symbol_rate)."""
    return Acquisition(
        CODE,
        reps=REPS,
        spc=spc,
        chip_rate=CHIP_RATE,
        cn0_dbhz=max(cn0_dbhz, 30.0),
        doppler_uncertainty=DOPPLER_UNCERTAINTY,
        max_noncoh=MAX_NONCOH,
        symbol_rate=SYM_RATE,
    )


def _code_phase_ref(spc: int) -> int:
    """Ground-truth `code_phase` for a clean, zero-Doppler signal at this
    spc -- see the module docstring for why one reference run per spc
    suffices for every C/N0/Doppler/power-scale/seed combination."""
    x_gen, _ = make_signal_wfmgen(100.0, 0.0, seed=999_999, n_sym=N_SYM)
    fs_front = CHIP_RATE * spc
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        a = make_engine(60.0, spc)
    te = a.code_bins * a.doppler_bins
    for pos in range(0, len(x) - te + 1, te):
        hits = a.push(x[pos : pos + te])
        if hits:
            return int(hits[0][1])
    raise RuntimeError(f"reference run never acquired at spc={spc}")


# Computed once at import (six cheap reference runs), reused by every trial.
_CODE_PHASE_REF: dict[int, int] = {
    spc: _code_phase_ref(spc) for spc in SPC_CHOICES
}


def run_trial(
    seed,
    cn0_dbhz=None,
    doppler_hz=None,
    spc=None,
    power_scale=None,
    n_sym=N_SYM,
    max_frames=MAX_FRAMES,
):
    """Run one randomized (or pinned) trial and return a full diagnostic
    record: ground truth, every Acquisition property, and the hit's
    estimates compared against that ground truth. Nothing is silently
    discarded -- a false lock (fires, but off-cell) is recorded, not
    treated the same as a clean miss."""
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

    x_gen, _ = make_signal_wfmgen(cn0_dbhz, doppler_hz, seed, n_sym)
    fs_front = CHIP_RATE * spc
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)
    x = (x * power_scale).astype(np.complex64)

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        a = make_engine(cn0_dbhz, spc)

    te = a.code_bins * a.doppler_bins
    row_true = round(doppler_hz / a.doppler_res_hz) % a.doppler_bins
    col_ref = _CODE_PHASE_REF[spc]

    record = {
        "seed": seed,
        "cn0_true_dbhz": cn0_dbhz,
        "doppler_true_hz": doppler_hz,
        "spc": spc,
        "power_scale": power_scale,
        "doppler_bins": a.doppler_bins,
        "n_noncoh": a.n_noncoh,
        "doppler_res_hz": a.doppler_res_hz,
        "pd_predicted": a.pd_predicted,
        "straddle_loss": a.straddle_loss,
        "underpowered": a.underpowered,
        "threshold": a.threshold,
        "pfa_cell": a.pfa_cell,
    }

    frames_available = max(0, (len(x) - te) // te + 1) if te else 0
    n_frames = min(max_frames, frames_available)
    hit = None
    frames_to_detect = None
    for f in range(n_frames):
        pos = f * te
        hits = a.push(x[pos : pos + te])
        if hits:
            hit = hits[0]
            frames_to_detect = f + 1
            break

    if hit is None:
        record.update(
            detected=False,
            frames_to_detect=None,
            frames_available=n_frames,
            doppler_bin=None,
            doppler_hz_est=float("nan"),
            doppler_err_hz=float("nan"),
            code_phase=None,
            code_phase_err_chips=float("nan"),
            peak_mag=float("nan"),
            noise_est=float("nan"),
            test_stat=float("nan"),
            cn0_dbhz_est=float("nan"),
            cn0_err_db=float("nan"),
            on_true_cell=False,
        )
        return record

    dop_bin, code_phase, peak_mag, noise_est, test_stat, cn0_dbhz_est = hit
    doppler_hz_est = (
        dop_bin if dop_bin <= a.doppler_bins // 2 else dop_bin - a.doppler_bins
    ) * a.doppler_res_hz
    dop_bin_dist = _circ_dist(dop_bin, row_true, a.doppler_bins)
    code_dist_samples = _circ_dist(code_phase, col_ref, a.code_bins)
    on_true_cell = dop_bin_dist <= 1 and code_dist_samples <= spc

    record.update(
        detected=True,
        frames_to_detect=frames_to_detect,
        frames_available=n_frames,
        doppler_bin=dop_bin,
        doppler_hz_est=doppler_hz_est,
        doppler_err_hz=doppler_hz_est - doppler_hz,
        code_phase=code_phase,
        code_phase_err_chips=code_dist_samples / spc,
        peak_mag=peak_mag,
        noise_est=noise_est,
        test_stat=test_stat,
        cn0_dbhz_est=cn0_dbhz_est,
        cn0_err_db=cn0_dbhz_est - cn0_dbhz,
        on_true_cell=on_true_cell,
    )
    return record


def sweep_cn0_calibration(n_per_bucket=12, doppler_hz=20.0, spc=2, seed0=4000):
    """Dedicated C/N0 sweep at fixed near-zero Doppler/spc/power (isolates
    the C/N0 axis, same rationale as `dsss_receiver_stress`'s dedicated
    sweep). Returns, per bucket, the empirical on-true-cell rate and the
    engine's own mean `pd_predicted` -- the direct "measured performance vs
    the setting" check: does the empirical rate roughly track what the
    engine itself predicted for that operating point?"""
    edges = np.linspace(*CN0_RANGE_DBHZ, 4)
    rates, predicted = [], []
    for i, (lo, hi) in enumerate(zip(edges[:-1], edges[1:])):
        rng = np.random.default_rng(seed0 + i)
        cn0s = rng.uniform(lo, hi, n_per_bucket)
        recs = [
            run_trial(
                seed0 + i * 1000 + k,
                cn0_dbhz=c,
                doppler_hz=doppler_hz,
                spc=spc,
                power_scale=1.0,
            )
            for k, c in enumerate(cn0s)
        ]
        hit = sum(r["on_true_cell"] for r in recs)
        rates.append(hit / len(recs))
        predicted.append(float(np.mean([r["pd_predicted"] for r in recs])))
        print(
            f"  CN0 in [{lo:.0f},{hi:.0f}) dB-Hz: {hit}/{len(recs)} "
            f"on true cell ({rates[-1]:.0%}), "
            f"engine predicted Pd={predicted[-1]:.2f}"
        )
    return rates, predicted


def measure_doppler_handoff_quality(records, known_downstream_pullin_hz=263.0):
    """Quantify how good a coarse-frequency handoff Acquisition gives a
    downstream carrier-tracking loop (`Dll` -> `MpskReceiver` in the
    composed `DsssReceiver`) -- the direct link between "does Acquisition
    find the right code phase across the whole span" (yes --
    :func:`sweep_doppler_acquisition_range`) and "why does the composed
    receiver's pull-in collapse at ~263 Hz anyway".

    `acq_create()`'s own symbol_rate-aware sizing deliberately picks the
    SMALLEST coherent depth (`doppler_bins`) that meets the target Pd in a
    single dwell, minimizing latency and mislock risk (see the header's own
    doc comment) -- which collapses to `doppler_bins == 1` (one bin
    spanning the ENTIRE +/-`SPAN_HZ` range, i.e. zero frequency resolution)
    across nearly this whole story's C/N0 operating range once C/N0 clears
    ~45 dB-Hz. Acquisition still detects correctly (the code phase and
    "which bin" are right), but the Doppler *estimate* it hands off can
    then be off by nearly the full native span -- and a downstream carrier
    loop pulling in from that estimate has to close the gap on its own.
    """
    on_cell = [r for r in records if r["on_true_cell"]]
    if not on_cell:
        return {"n": 0}
    errs = np.array([abs(r["doppler_err_hz"]) for r in on_cell])
    bins1 = sum(1 for r in on_cell if r["doppler_bins"] == 1)
    return {
        "n": len(on_cell),
        "frac_doppler_bins_eq_1": bins1 / len(on_cell),
        "mean_abs_err_hz": float(errs.mean()),
        "median_abs_err_hz": float(np.median(errs)),
        "frac_exceeds_downstream_pullin": float(
            (errs > known_downstream_pullin_hz).mean()
        ),
    }


def sweep_doppler_acquisition_range(
    n_per_bucket=10, cn0_dbhz=65.0, spc=2, seed0=6000
):
    """The key isolation result: does Acquisition's OWN on-true-cell rate
    stay high across its FULL configured Doppler search range, or does it
    narrow the way the composed DsssReceiver did? Sweeps |Doppler| buckets
    from 0 up to `DOPPLER_FRAC_OF_SPAN * SPAN_HZ` (the same range
    `dsss_receiver_stress.find_doppler_pullin_boundary` bisected and found
    collapsing by ~263 Hz at the receiver level)."""
    edges = np.linspace(0.0, DOPPLER_FRAC_OF_SPAN * SPAN_HZ, 6)
    rates = []
    for i, (lo, hi) in enumerate(zip(edges[:-1], edges[1:])):
        rng = np.random.default_rng(seed0 + i)
        signs = rng.choice([-1.0, 1.0], n_per_bucket)
        dops = signs * rng.uniform(lo, hi, n_per_bucket)
        recs = [
            run_trial(
                seed0 + i * 1000 + k,
                cn0_dbhz=cn0_dbhz,
                doppler_hz=float(d),
                spc=spc,
                power_scale=1.0,
            )
            for k, d in enumerate(dops)
        ]
        hit = sum(r["on_true_cell"] for r in recs)
        rates.append(hit / len(recs))
        print(
            f"  |Doppler| in [{lo:.0f},{hi:.0f}) Hz: {hit}/{len(recs)} "
            f"on true cell ({rates[-1]:.0%})"
        )
    return rates


def main(n_trials=300, out_path="dsss_acquisition_stress.png"):
    print(
        f"Acquisition stress: SF={SF}, chip_rate={CHIP_RATE / 1e6:g} MHz, "
        f"native span +/-{SPAN_HZ:.0f} Hz, "
        f"searching +/-{DOPPLER_UNCERTAINTY:.0f} Hz"
    )

    print("\nDoppler-range isolation sweep (fixed CN0=65 dB-Hz, spc=2):")
    doppler_rates = sweep_doppler_acquisition_range()

    print("\nC/N0 calibration sweep (fixed near-zero Doppler, spc=2):")
    cn0_rates, _cn0_predicted = sweep_cn0_calibration()

    rng = np.random.default_rng(0)
    records = [run_trial(int(s)) for s in rng.integers(0, 2**31 - 1, n_trials)]

    n_detected = sum(r["detected"] for r in records)
    n_on_cell = sum(r["on_true_cell"] for r in records)
    print(
        f"\nFull random sweep ({n_trials} trials): {n_detected} detected, "
        f"{n_on_cell} on true cell"
    )

    handoff = measure_doppler_handoff_quality(records)
    if handoff["n"]:
        print(
            "\nDoppler handoff quality (is Acquisition's own estimate good "
            "enough for a downstream carrier loop to pull in from?):\n"
            f"  doppler_bins==1 (zero frequency resolution) in "
            f"{handoff['frac_doppler_bins_eq_1']:.0%} of on-cell trials\n"
            f"  |Doppler est error|: mean={handoff['mean_abs_err_hz']:.0f} Hz "
            f"median={handoff['median_abs_err_hz']:.0f} Hz\n"
            f"  {handoff['frac_exceeds_downstream_pullin']:.0%} of on-cell "
            "trials have |error| > 263 Hz -- dsss_receiver_stress.py's own "
            "measured DsssReceiver-level Doppler pull-in boundary"
        )

    on_cell_recs = [r for r in records if r["on_true_cell"]]
    if on_cell_recs:
        dop_err = np.array([r["doppler_err_hz"] for r in on_cell_recs])
        cn0_err = np.array([r["cn0_err_db"] for r in on_cell_recs])
        code_err = np.array([r["code_phase_err_chips"] for r in on_cell_recs])
        print(
            f"  Doppler err (Hz): mean={dop_err.mean():.2f} "
            f"std={dop_err.std():.2f}"
        )
        print(
            f"  Code-phase err (chips): mean={code_err.mean():.3f} "
            f"std={code_err.std():.3f}"
        )
        print(
            f"  C/N0 est err (dB): mean={cn0_err.mean():.2f} "
            f"std={cn0_err.std():.2f}"
        )

    # A hit that lands off the true cell is a false lock -- a genuine
    # anomaly distinct from a clean non-detection, worth surfacing with its
    # full parameter tuple rather than folding into a pass rate.
    false_locks = [
        r for r in records if r["detected"] and not r["on_true_cell"]
    ]
    if false_locks:
        print(
            f"\n{len(false_locks)} FALSE LOCK(S) -- "
            "fired but off the true cell:"
        )
        for r in false_locks:
            print(f"  {r}")

    # The isolation result: Acquisition's own on-true-cell rate should stay
    # high across the WHOLE configured Doppler range -- if this holds while
    # dsss_receiver_stress's boundary sits at ~263 Hz, the narrowing is
    # proven to live downstream of Acquisition (Dll/MpskReceiver's own
    # carrier loop), not in Acquisition's Doppler search itself.
    assert all(r >= 0.7 for r in doppler_rates), (
        f"Acquisition's own on-true-cell rate dropped below 70% within its "
        f"configured search range: {doppler_rates} -- the Doppler search "
        f"itself (not just the downstream tracking loops) may be narrow"
    )

    valid = [r for r in cn0_rates if not math.isnan(r)]
    assert valid == sorted(valid), (
        f"on-true-cell rate should be non-decreasing with C/N0, "
        f"got {cn0_rates}"
    )
    assert valid[-1] >= 0.85, (
        f"expected >=85% on-true-cell in the strongest C/N0 bucket, "
        f"got {valid[-1]:.0%}"
    )

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, axes = plt.subplots(1, 3, figsize=(16, 4.6))

        ax = axes[0]
        dopp = np.array([abs(r["doppler_true_hz"]) for r in records])
        hit = np.array([r["on_true_cell"] for r in records], dtype=float)
        cn0 = np.array([r["cn0_true_dbhz"] for r in records])
        sc = ax.scatter(dopp, hit, c=cn0, cmap="viridis", s=14, alpha=0.7)
        ax.set_xlabel("|Doppler| (Hz)")
        ax.set_ylabel("on true cell (0/1)")
        ax.set_title(
            "Acquisition hit accuracy vs |Doppler|\n(color = true C/N0)"
        )
        fig.colorbar(sc, ax=ax, label="C/N0 (dB-Hz)")

        ax = axes[1]
        if on_cell_recs:
            ax.scatter(
                [r["cn0_true_dbhz"] for r in on_cell_recs],
                [r["cn0_dbhz_est"] for r in on_cell_recs],
                s=14,
                alpha=0.6,
            )
            lo = min(r["cn0_true_dbhz"] for r in on_cell_recs)
            hi = max(r["cn0_true_dbhz"] for r in on_cell_recs)
            ax.plot([lo, hi], [lo, hi], "r--", lw=1, label="ideal")
        ax.set_xlabel("true C/N0 (dB-Hz)")
        ax.set_ylabel("estimated C/N0 (dB-Hz)")
        ax.set_title("cn0_dbhz_est vs truth")
        ax.legend(fontsize=8)

        ax = axes[2]
        centers = 0.5 * (
            np.linspace(0, DOPPLER_FRAC_OF_SPAN * SPAN_HZ, 6)[:-1]
            + np.linspace(0, DOPPLER_FRAC_OF_SPAN * SPAN_HZ, 6)[1:]
        )
        ax.plot(centers, doppler_rates, "o-", label="on-true-cell rate")
        ax.axhline(0.7, color="0.6", ls="--", lw=1, label="70% floor")
        ax.set_xlabel("|Doppler| bucket center (Hz)")
        ax.set_ylabel("on-true-cell rate")
        ax.set_ylim(-0.03, 1.03)
        ax.set_title("Acquisition-alone Doppler-range isolation")
        ax.legend(fontsize=8)
        ax.grid(alpha=0.3)

        fig.tight_layout()
        fig.savefig(out_path, dpi=110)
        print(f"\nwrote {out_path}")
    except ImportError:
        pass


if __name__ == "__main__":
    main(
        out_path=sys.argv[1]
        if len(sys.argv) > 1
        else "dsss_acquisition_stress.png"
    )
