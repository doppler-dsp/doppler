"""Stage-by-stage diagnostic for task #148 (`async_dsss_receiver_c_vs_
python.py`'s case 1: a STATIC +15kHz Doppler offset, `aid_code=False`,
Python `e2e_acq_to_despreader.run_trial` PASSES, both C receivers
(`DsssReceiver` and `AsyncDsssReceiver`) FAIL at chance-level BER).

Rather than one final BER number, this instruments every stage of BOTH
pipelines on the IDENTICAL signal and compares them directly:

  1. **Acquisition hit** (coarse Doppler estimate, cn0_dbhz_est). Both
     pipelines run through the literal SAME C `acq_core.c` engine
     (`doppler.dsss.Acquisition` and `AsyncDsssReceiver`'s embedded
     `acq_state_t*` are the same compiled code) -- expected to agree
     closely; a mismatch here would mean the bug is upstream of
     everything this story has already validated.
  2. **Fine carrier measurement** (CarrierAcquisition's residual_hz /
     refined Doppler estimate). Both pipelines ALSO run through the
     literal same C `carrier_acq_core.c` engine (Python's
     `freq_refine.refine_seed_carrier_acq` calls `doppler.acquire.
     CarrierAcquisition` directly; `AsyncDsssReceiver`'s embedded
     `carrier_acq_state_t*` is the same object). Expected to agree
     closely too.
  3. **Tracking-loop stress**: per-symbol EVM over time once tracking
     begins. THIS is where the two pipelines structurally diverge --
     Python's `e2e_acq_to_despreader.py` deliberately stops at the
     despread OUTPUT (`CoupledAsyncDespreader.run()`), never running a
     downstream `MpskReceiver`-equivalent symbol-timing/demod stage;
     the C receivers do. If (1) and (2) agree but (3) shows the C
     receiver's EVM catastrophically blows up right at tracking onset
     (not a gradual drift), the bug lives in what the C receivers do
     with the Doppler estimate AT the track-chain handoff -- not in the
     estimate itself.

**Working hypothesis going in** (see `_build_track_chain` in both
`dsss_receiver_core.c` and `async_dsss_receiver_core.c`): `mpsk_
receiver_create()`'s own `norm_freq` init argument is seeded with the
SAME full physical `doppler_hz_est` that ALSO seeds the pre-despread
Costas loop (`doppler_hz_est / front_end_rate`), but evaluated at
`target_rate = sps*symbol_rate` (e.g. 8*2700=21600 Hz) instead of the
front-end rate (e.g. chip_rate*spc=6.138e6 Hz). If the pre-despread
carrier loop already derotates away nearly the full Doppler BEFORE the
signal ever reaches MpskReceiver, then MpskReceiver's own carrier NCO
should be seeded near 0 (a small residual), not the full estimate again
-- a double count. At small Doppler (a few hundred Hz) this is
negligible either way (doppler_hz_est/target_rate is tiny). At 15000 Hz
static offset, 15000/21600 = 0.694 cycles/sample -- past Nyquist (0.5),
aliasing to roughly -0.306 cycles/sample: a large, WRONG initial
condition for MpskReceiver's own carrier tracking loop, plausibly
outside its pull-in range.

Run: `python case1_stage_diagnostics.py` (needs numpy + matplotlib +
doppler). Writes `case1_hit_and_refine.png` and `case1_evm_stress.png`
into this folder.
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from acq_handoff import search_and_handoff
from despreader_coupled import CoupledAsyncDespreader
from freq_refine import refine_seed_carrier_acq
from spec_full_characterization import (
    CHIP_RATE,
    CODE,
    DOPPLER_UNCERTAINTY_HZ,
    FS_GEN,
    SF,
    SPC,
    SYM_RATE,
    es_n0_to_cn0_dbhz,
    make_ramp_signal,
)

from doppler.dsss import Acquisition, AsyncDsssReceiver
from doppler.resample import RateConverter

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
BN_CODE = 0.002
BN_CARRIER = 0.01
TRACK_WINDOWS = 6
N_SYM = 8000
PREFIX_EPOCHS = 2700
ESN0_DB = 10.0
STATIC_DOPPLER_HZ = 15000.0
SEED = 8001


def _fit_and_windowed_evm(vals, idxs, data_bits, max_lag=150, window=100):
    """Best-(lag, sign, gain) LS fit over the FULL record (same
    normalization `spec_full_characterization._lag_search_metrics`
    uses), then a sliding-window RMS-EVM(dB) trace at that ONE fixed
    (lag, sign, gain) -- so a loop that locks well initially and then
    drifts/unlocks shows up as a rising trace, not hidden inside one
    global average. `vals`/`idxs` are (complex per-symbol value, its
    true-bit-index) pairs; works identically whether `vals` came from a
    real `MpskReceiver` (C) or a genie-aided per-bit accumulation of a
    despread-output stream (Python, see `_per_bit_values` below) -- the
    metric doesn't care which produced it, only that both are one
    complex sample per known data bit.
    """
    n = len(vals)
    if n < 20:
        return None
    lo, hi = n // 3, 2 * n // 3
    best_err = np.inf
    best = None
    for lag in range(-max_lag, max_lag + 1):
        ti = idxs[lo:hi] + lag
        mask = (ti >= 0) & (ti < len(data_bits))
        if mask.sum() < (hi - lo) // 2:
            continue
        truth = np.where(data_bits[ti[mask]] > 0, -1.0, 1.0)
        z = vals[lo:hi][mask]
        for sign in (1.0, -1.0):
            ref = sign * truth
            gain = np.mean(np.conj(ref) * z)
            if np.abs(gain) < 1e-12:
                continue
            err = z / gain - ref
            e = float(np.mean(np.abs(err) ** 2))
            if e < best_err:
                best_err = e
                best = (lag, sign, gain)
    if best is None:
        return None
    lag, sign, gain = best

    ti = idxs + lag
    mask = (ti >= 0) & (ti < len(data_bits))
    ti_m = ti[mask]
    z = vals[mask]
    ref = sign * np.where(data_bits[ti_m] > 0, -1.0, 1.0)
    evm_inst = np.abs(z / gain - ref)

    n2 = len(evm_inst)
    centers, evm_db = [], []
    step = max(1, window // 2)
    for start in range(0, max(n2 - window, 1), step):
        seg = evm_inst[start : start + window]
        if len(seg) < window // 2:
            continue
        rms = float(np.sqrt(np.mean(seg**2)))
        evm_db.append(20.0 * np.log10(rms + 1e-12))
        centers.append(start + len(seg) // 2)
    return {
        "lag": lag,
        "sign": sign,
        "gain": gain,
        "overall_evm_db": 20.0 * np.log10(np.sqrt(best_err) + 1e-12),
        "centers": np.array(centers),
        "evm_db": np.array(evm_db),
    }


def _per_bit_values(out, samples_per_bit, frac_bit_offset, n_bits):
    """Genie-aided per-bit COMPLEX accumulation (mean over each known
    true bit's own sample window) -- `despread_output_ber`'s own slicing
    convention, but keeping the complex mean instead of collapsing to a
    sign, so the result is comparable to a real MpskReceiver's own
    per-symbol soft output for EVM purposes."""
    idxs, vals = [], []
    for i in range(n_bits):
        i0 = int(round((i - frac_bit_offset) * samples_per_bit))
        i1 = int(round((i + 1 - frac_bit_offset) * samples_per_bit))
        if i0 < 0 or i1 > len(out) or i1 <= i0:
            continue
        vals.append(out[i0:i1].mean())
        idxs.append(i)
    return np.array(idxs), np.array(vals, dtype=np.complex128)


def run_python_case1():
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz, SEED, n_sym=N_SYM, rate_hz_per_s=0.0,
        static_doppler_hz=STATIC_DOPPLER_HZ,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen)

    acq = Acquisition(
        CODE, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ, pfa=1e-3, pd=0.9,
        symbol_rate=SYM_RATE,
    )
    handoff, consumed = search_and_handoff(acq, x, SPC, FS_FRONT)
    coarse_err_hz = handoff.doppler_hz_est - STATIC_DOPPLER_HZ
    tracker_init_chip = (SF - handoff.chip_phase) % SF
    coarse_norm_freq = handoff.doppler_hz_est / FS_FRONT

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = min(PREFIX_EPOCHS, n_epochs_total // 4)
    prefix = tail[: n_prefix_epochs * TE]

    cn0_min_dbhz = es_n0_to_cn0_dbhz(5.0, sym_rate=SYM_RATE)
    refined_norm_freq, residual_hz, ca_ready, samples_consumed = (
        refine_seed_carrier_acq(
            CoupledAsyncDespreader, CODE, SPC, BN_CODE, coarse_norm_freq,
            prefix, FS_FRONT, SYM_RATE, cn0_min_dbhz, bn_car=BN_CARRIER,
            windows=11, init_chip=tracker_init_chip,
        )
    )
    n_epochs_used = min(-(-samples_consumed // TE) or 1, n_prefix_epochs)
    track_rx = tail[n_epochs_used * TE : n_epochs_total * TE]
    refined_err_hz = refined_norm_freq * FS_FRONT - STATIC_DOPPLER_HZ

    d = CoupledAsyncDespreader(
        CODE, SPC, bn=BN_CODE, zeta=0.707, spacing=0.5,
        windows=TRACK_WINDOWS, init_chip=tracker_init_chip,
        init_car_norm_freq=refined_norm_freq, aid_code=False,
        sample_rate_hz=FS_FRONT, bn_car=BN_CARRIER, bn_fll_car=0.0,
        car_update_windows=True,
    )
    out = d.run(track_rx)

    out_rate_hz = CHIP_RATE * TRACK_WINDOWS / SF
    elapsed_bits = (consumed + n_epochs_used * TE) / FS_FRONT * SYM_RATE
    symbols_in_prefix = int(np.floor(elapsed_bits))
    frac_bit_offset = elapsed_bits - symbols_in_prefix
    data_bits_tail = data_bits[symbols_in_prefix:]
    samples_per_bit = out_rate_hz / SYM_RATE

    idxs, vals = _per_bit_values(
        out, samples_per_bit, frac_bit_offset, len(data_bits_tail)
    )
    evm = _fit_and_windowed_evm(vals, idxs, data_bits_tail)
    # Convert bit-index-based window centers to elapsed seconds since
    # tracking (out) onset, for a fair x-axis against the C trace.
    time_s = evm["centers"] / SYM_RATE if evm else np.zeros(0)

    return {
        "coarse_doppler_hz": handoff.doppler_hz_est,
        "coarse_err_hz": coarse_err_hz,
        "cn0_dbhz_est": handoff.cn0_dbhz_est,
        "refined_doppler_hz": refined_norm_freq * FS_FRONT,
        "refined_err_hz": refined_err_hz,
        "residual_hz": residual_hz,
        "ca_ready": ca_ready,
        "evm": evm,
        "time_s": time_s,
    }


def run_c_case1():
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz, SEED, n_sym=N_SYM, rate_hz_per_s=0.0,
        static_doppler_hz=STATIC_DOPPLER_HZ,
    )
    x = (
        RateConverter(rate=FS_FRONT / FS_GEN)
        .execute(x_gen)
        .astype(np.complex64)
    )

    rx = AsyncDsssReceiver(
        CODE, chip_rate=CHIP_RATE, symbol_rate=SYM_RATE, spc=SPC,
        cn0_dbhz=cn0_dbhz, doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        segments=4, sps=8,
    )

    coarse = None
    refined = None
    sym_chunks = []
    sym_starts = []  # cumulative symbol count BEFORE each chunk
    n_syms_so_far = 0

    for pos in range(0, len(x) - TE, TE):
        was_refining, was_tracking = rx.refining, rx.tracking
        out = rx.steps(x[pos : pos + TE])
        if rx.refining and not was_refining and coarse is None:
            coarse = {
                "doppler_hz": rx.doppler_hz,
                "cn0_dbhz_est": rx.cn0_dbhz_est,
            }
        if rx.tracking and not was_tracking and refined is None:
            refined = {"doppler_hz": rx.doppler_hz}
        if len(out):
            sym_chunks.append(out)
            sym_starts.append(n_syms_so_far)
            n_syms_so_far += len(out)

    syms = (
        np.concatenate(sym_chunks)
        if sym_chunks
        else np.zeros(0, dtype=np.complex64)
    )
    idxs = np.arange(len(syms))
    evm = _fit_and_windowed_evm(
        syms.astype(np.complex128), idxs, data_bits.astype(np.float64)
    )
    time_s = evm["centers"] / SYM_RATE if evm else np.zeros(0)

    coarse = coarse or {"doppler_hz": float("nan"), "cn0_dbhz_est": float("nan")}
    refined = refined or {"doppler_hz": float("nan")}
    return {
        "coarse_doppler_hz": coarse["doppler_hz"],
        "coarse_err_hz": coarse["doppler_hz"] - STATIC_DOPPLER_HZ,
        "cn0_dbhz_est": coarse["cn0_dbhz_est"],
        "refined_doppler_hz": refined["doppler_hz"],
        "refined_err_hz": refined["doppler_hz"] - STATIC_DOPPLER_HZ,
        "final_tracking": bool(rx.tracking),
        "final_lock": rx.lock,
        "final_norm_freq": rx.norm_freq,
        "evm": evm,
        "time_s": time_s,
    }


def _bar_compare(ax, labels, py_val, c_val, title, ylabel):
    x = np.arange(len(labels))
    w = 0.35
    ax.bar(x - w / 2, py_val, w, label="Python", color="tab:blue")
    ax.bar(x + w / 2, c_val, w, label="C", color="tab:orange")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    ax.axhline(0, color="k", lw=0.5)
    ax.legend()
    ax.grid(True, alpha=0.3)


def main():
    print("Running Python e2e pipeline (case 1: static +15kHz, aid_code=False)...")
    py = run_python_case1()
    print("Running C AsyncDsssReceiver pipeline (identical signal)...")
    c = run_c_case1()

    print("\n=== Acquisition hit ===")
    print(f"  Python: doppler_hz_est={py['coarse_doppler_hz']:.1f}  "
          f"err={py['coarse_err_hz']:+.1f} Hz  cn0_dbhz_est={py['cn0_dbhz_est']:.2f}")
    print(f"  C:      doppler_hz_est={c['coarse_doppler_hz']:.1f}  "
          f"err={c['coarse_err_hz']:+.1f} Hz  cn0_dbhz_est={c['cn0_dbhz_est']:.2f}")

    print("\n=== Fine carrier (CarrierAcquisition) ===")
    print(f"  Python: refined={py['refined_doppler_hz']:.1f} Hz  "
          f"err={py['refined_err_hz']:+.1f} Hz  ready={py['ca_ready']}")
    print(f"  C:      refined={c['refined_doppler_hz']:.1f} Hz  "
          f"err={c['refined_err_hz']:+.1f} Hz  tracking={c['final_tracking']}  "
          f"lock={c['final_lock']:.3f}")

    print("\n=== Tracking-loop EVM ===")
    if py["evm"]:
        print(f"  Python overall EVM: {py['evm']['overall_evm_db']:.2f} dB")
    if c["evm"]:
        print(f"  C      overall EVM: {c['evm']['overall_evm_db']:.2f} dB")

    # ── Figure 1: acquisition hit + fine carrier measurement ──
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.2))
    _bar_compare(
        axes[0], ["coarse (acq)", "refined (carrier_acq)"],
        [py["coarse_err_hz"], py["refined_err_hz"]],
        [c["coarse_err_hz"], c["refined_err_hz"]],
        "Doppler estimate error vs. true 15 kHz offset", "error (Hz)",
    )
    _bar_compare(
        axes[1], ["cn0_dbhz_est"], [py["cn0_dbhz_est"]], [c["cn0_dbhz_est"]],
        "Acquisition hit C/N0 estimate", "dB-Hz",
    )
    labels = ["overall EVM"]
    py_evm = [py["evm"]["overall_evm_db"]] if py["evm"] else [0.0]
    c_evm = [c["evm"]["overall_evm_db"]] if c["evm"] else [0.0]
    _bar_compare(
        axes[2], labels, py_evm, c_evm,
        "Tracking-stage overall EVM (lower=better)", "EVM (dB)",
    )
    fig.suptitle(
        "Case 1 (static +15kHz offset): C vs Python, per-stage measurements"
    )
    fig.tight_layout()
    fig.savefig("case1_hit_and_refine.png", dpi=130)
    print("\nWrote case1_hit_and_refine.png")

    # ── Figure 2: EVM(t) during tracking -- the "loop stress" trace ──
    fig2, ax = plt.subplots(figsize=(9, 5))
    if py["evm"]:
        ax.plot(
            py["time_s"], py["evm"]["evm_db"], label="Python "
            "(CoupledAsyncDespreader, despread output)", color="tab:blue",
        )
    if c["evm"]:
        ax.plot(
            c["time_s"], c["evm"]["evm_db"], label="C (AsyncDsssReceiver, "
            "MpskReceiver symbols)", color="tab:orange",
        )
    ax.axhline(-ESN0_DB, color="k", ls="--", lw=1, label=f"AWGN floor (-{ESN0_DB:.0f} dB)")
    ax.set_xlabel("time since tracking onset (s)")
    ax.set_ylabel("windowed EVM (dB)")
    ax.set_title(
        "Case 1 (static +15kHz offset, aid_code=False): tracking-loop stress"
    )
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig2.tight_layout()
    fig2.savefig("case1_evm_stress.png", dpi=130)
    print("Wrote case1_evm_stress.png")


if __name__ == "__main__":
    main()
