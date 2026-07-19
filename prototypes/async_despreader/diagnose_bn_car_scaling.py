"""Regression proof for CHECKPOINT 25's finding (`FINISHING_PLAN.md`):
`car_update_windows=True`'s own `bn_car` must be passed UNCHANGED, not
divided by `windows`/`TRACK_WINDOWS` -- doing so under-scales the
carrier loop's integrator (the mechanism that gives a type-2 loop its
ramp/rate-tracking capability) and made the real SPEC Doppler rate
(500Hz/s) untrackable (BER~0.5). No FLL-assist is needed once this is
fixed -- matching `~/legacy-commz`'s own reference architecture, which
also tracks a real Doppler rate with a plain Costas PLL, no FLL
cross-product term at all (the user's own direct call: "No FLL needed
-- legacy proves that").

Part 1 -- isolates WHY the division was wrong: queries `doppler.track
.LoopFilter` directly across an 8x `bn` sweep. `kp` scales ~linearly
with `bn`; `ki` scales ~QUADRATICALLY (`ki/bn**2` stays ~constant) --
the standard 2nd-order PI loop filter design. Dividing `bn_car` by
`windows` to "keep the same real bandwidth" only holds for the linear
`kp` term; it silently under-scales `ki` by an EXTRA factor of
`windows`, on top of the `windows`-times-more-frequent updates.

Part 2 -- proves the fix on the real signal chain: runs the SAME
Acquisition -> handoff -> PSDMF pipeline as `e2e_acq_to_despreader.py`
on the real 500Hz/s SPEC rate case, then tracks with `car_update_
windows=True` twice -- once with `bn_car` wrongly divided by
`TRACK_WINDOWS` (the pre-fix bug), once with `bn_car` passed straight
through (the fix, now what `e2e_acq_to_despreader.py` itself does).
Writes per-epoch telemetry to `logs/*_telemetry.csv` and a comparison
plot to `logs/bn_car_scaling_fix.png` (both gitignored, this folder's
own persistent `logs/` directory, not scratch).

Run: `python diagnose_bn_car_scaling.py` (needs numpy + doppler +
matplotlib).
"""

from __future__ import annotations

import csv
import os

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from acq_handoff import search_and_handoff
from despreader_coupled import CoupledAsyncDespreader
from freq_refine import refine_seed_matched
from doppler.track import LoopFilter
from spec_full_characterization import (
    CODE,
    CHIP_RATE,
    DOPPLER_UNCERTAINTY_HZ,
    FS_GEN,
    RATE_HZ_PER_S,
    SF,
    SPC,
    SYM_RATE,
    es_n0_to_cn0_dbhz,
    make_ramp_signal,
)

from doppler.dsss import Acquisition
from doppler.resample import RateConverter

LOG_DIR = os.path.join(os.path.dirname(__file__), "logs")
os.makedirs(LOG_DIR, exist_ok=True)

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
BN_CODE = 0.002
BN_CARRIER = 0.01
WINDOWS = 62
TRACK_WINDOWS = 6
N_SYM = 8000
PREFIX_EPOCHS = 2700
ESN0_DB = 10.0
SEED = 6000


def part1_loopfilter_scaling():
    print("=== Part 1: doppler.track.LoopFilter's own bn -> kp/ki scaling ===")
    for bn in (0.01, 0.02, 0.04, 0.08):
        lf = LoopFilter(bn=bn, zeta=0.707, t=1.0)
        print(
            f"  bn={bn:<6} kp={lf.kp:.6f} (kp/bn={lf.kp / bn:.3f})   "
            f"ki={lf.ki:.8f} (ki/bn**2={lf.ki / bn**2:.3f})"
        )
    print(
        "  kp/bn ~constant (linear), ki/bn**2 ~constant (quadratic) -- "
        "dividing bn_car by windows under-scales ki by an extra factor "
        "of windows.\n"
    )


def prepare_common():
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz, SEED, n_sym=N_SYM, rate_hz_per_s=RATE_HZ_PER_S,
        static_doppler_hz=0.0,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen)

    acq = Acquisition(
        CODE, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ, pfa=1e-3, pd=0.9,
        symbol_rate=SYM_RATE,
    )
    handoff, consumed = search_and_handoff(acq, x, SPC, FS_FRONT)

    def true_doppler_at(n_front_samples):
        return RATE_HZ_PER_S * (n_front_samples / FS_FRONT)

    tracker_init_chip = (SF - handoff.chip_phase) % SF
    coarse_norm_freq = handoff.doppler_hz_est / FS_FRONT

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = min(PREFIX_EPOCHS, n_epochs_total // 4)
    prefix = tail[: n_prefix_epochs * TE]
    n_epochs_track = n_epochs_total - n_prefix_epochs
    track_rx = tail[n_prefix_epochs * TE : n_epochs_total * TE]
    t0_samples = consumed + n_prefix_epochs * TE  # true_doppler_at's
    # own n_front_samples must be measured from x's t=0, not tracking's
    # own t=0 -- the ramp already accumulated this many samples' worth
    # of drift during acquisition + the PSDMF prefix.

    refined_norm_freq, _residual_hz = refine_seed_matched(
        CoupledAsyncDespreader, CODE, SPC, BN_CODE, coarse_norm_freq,
        prefix, FS_FRONT, SYM_RATE, n_fft=64, zero_pad=4, interp=True,
        bn_car=BN_CARRIER, windows=WINDOWS, init_chip=tracker_init_chip,
    )
    return {
        "track_rx": track_rx,
        "n_epochs_track": n_epochs_track,
        "tracker_init_chip": tracker_init_chip,
        "refined_norm_freq": refined_norm_freq,
        "true_doppler_at": true_doppler_at,
        "t0_samples": t0_samples,
    }


def run_config(label, common, bn_car):
    d = CoupledAsyncDespreader(
        CODE, SPC, bn=BN_CODE, zeta=0.707, spacing=0.5,
        windows=TRACK_WINDOWS, init_chip=common["tracker_init_chip"],
        init_car_norm_freq=common["refined_norm_freq"], aid_code=False,
        sample_rate_hz=FS_FRONT, bn_car=bn_car, bn_fll_car=0.0,
        car_update_windows=True,
    )
    n_epochs_track = common["n_epochs_track"]
    track_rx = common["track_rx"]
    true_doppler_at = common["true_doppler_at"]
    t0_samples = common["t0_samples"]

    t_s = np.empty(n_epochs_track)
    car_hz = np.empty(n_epochs_track)
    true_hz = np.empty(n_epochs_track)
    for k in range(n_epochs_track):
        d.run(track_rx[k * TE : (k + 1) * TE])
        t_s[k] = k * TE / FS_FRONT
        car_hz[k] = d.car_norm_freq * FS_FRONT
        true_hz[k] = true_doppler_at(t0_samples + (k + 1) * TE)

    csv_path = os.path.join(LOG_DIR, f"{label}_telemetry.csv")
    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s", "car_norm_freq_hz", "true_doppler_hz"])
        w.writerows(zip(t_s, car_hz, true_hz))
    print(f"  wrote {csv_path} ({n_epochs_track} rows)")
    return {"t_s": t_s, "car_hz": car_hz, "true_hz": true_hz}


def part2_fix_on_real_signal():
    print("=== Part 2: bn_car divided (bug) vs unchanged (fix), real 500Hz/s rate ===")
    common = prepare_common()

    print("[buggy] bn_car = BN_CARRIER / TRACK_WINDOWS")
    buggy = run_config("bn_car_divided_buggy", common, BN_CARRIER / TRACK_WINDOWS)

    print("[fixed] bn_car = BN_CARRIER (unchanged)")
    fixed = run_config("bn_car_unchanged_fixed", common, BN_CARRIER)

    for label, r in (("buggy (divided)", buggy), ("fixed (unchanged)", fixed)):
        err = r["car_hz"] - r["true_hz"]
        print(
            f"  {label}: final err={err[-1]:+.1f}Hz  "
            f"mean|err|={np.mean(np.abs(err)):.1f}Hz  "
            f"max|err|={np.max(np.abs(err)):.1f}Hz"
        )

    fig, ax = plt.subplots(2, 1, figsize=(11, 8), sharex=True)
    ax[0].plot(buggy["t_s"], buggy["car_hz"], label="buggy: bn_car/TRACK_WINDOWS")
    ax[0].plot(fixed["t_s"], fixed["car_hz"], label="fixed: bn_car unchanged")
    ax[0].plot(
        fixed["t_s"], fixed["true_hz"], "--", color="black",
        label="true Doppler",
    )
    ax[0].legend()
    ax[0].grid()
    ax[0].set_ylabel("Hz")
    ax[1].plot(buggy["t_s"], buggy["car_hz"] - buggy["true_hz"], label="buggy error")
    ax[1].plot(fixed["t_s"], fixed["car_hz"] - fixed["true_hz"], label="fixed error")
    ax[1].axhline(0, color="grey", lw=0.7)
    ax[1].legend()
    ax[1].grid()
    ax[1].set_xlabel("time since tracking start (s)")
    ax[1].set_ylabel("tracking error (Hz)")
    plt.suptitle(
        "car_update_windows=True carrier loop -- bn_car divided (bug) "
        "vs unchanged (fix), real 500Hz/s Doppler rate"
    )
    plt.tight_layout()
    png_path = os.path.join(LOG_DIR, "bn_car_scaling_fix.png")
    plt.savefig(png_path, dpi=150)
    print(f"  wrote {png_path}")


if __name__ == "__main__":
    part1_loopfilter_scaling()
    part2_fix_on_real_signal()
