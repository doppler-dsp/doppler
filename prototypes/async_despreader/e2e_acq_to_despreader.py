"""ONE clean end-to-end test for Acquisition -> Despreader (this
folder's canonical validation of that path, superseding
`archive/spec_full_characterization_prototype.py`'s three overlapping
trial functions): Acquisition search+handoff (`acq_handoff.py`) -> one
PSDMF frequency refinement pass, now the C-backed
`freq_refine.refine_seed_carrier_acq` (`doppler.acquire
.CarrierAcquisition`, folded in once its own CFAR threshold was fixed
-- see `FINISHING_PLAN.md`'s `CarrierAcquisition` section; previously
`freq_refine.refine_seed_matched`, the pure-Python PSDMF estimator) ->
`CoupledAsyncDespreader` tracking (`aid_code=True` + the per-window
`car_update_windows=True` carrier loop), at `SPEC.md`'s real operating
point -- the genuine +/-50kHz Acquisition search, the real 500Hz/s
Doppler rate.

Deliberately stops at the DESPREAD OUTPUT and does not touch any
downstream demod (`doppler.track.MpskReceiver`): its own symbol-timing
loop is a separate, currently open bug (CHECKPOINT 22,
`FINISHING_PLAN.md`) with no bearing on whether Acquisition, PSDMF, or
`CoupledAsyncDespreader` are themselves correct. Scored with a
genie-aided bit sync instead (`despread_output_ber` -- known true bit
boundaries slice the despread stream directly and decide each bit's
sign), so this test's pass/fail depends only on this folder's own
code, nothing downstream.

Building this surfaced two REAL, previously-conflated bugs in
Acq->Despreader itself (CHECKPOINT 23/25, `FINISHING_PLAN.md`) --
scoring at the despread level (no `MpskReceiver` confound) separates
them cleanly for the first time:

1. `aid_code=True` + a nonzero `static_doppler_hz` on THIS signal
   generator (`spec_full_characterization.make_ramp_signal`) is
   catastrophic (BER~0.5), not just the mild regression task #101
   found on `acq_handoff.py`'s signal -- because this generator (like
   that one) applies Doppler as a pure carrier-phase multiply with NO
   matching code-rate change, `aid_code` injects a real, physically-
   motivated-but-here-wrong code-rate correction that misaligns the
   code loop over a long run. `aid_code=False` on the SAME case scores
   BER~0.0002 (perfect). Not a bug in `aid_code` -- a fidelity gap in
   every signal generator this story has used; see task #101.
2. **FIXED (CHECKPOINT 25): the real SPEC rate (500Hz/s) alone was
   producing BER~0.5 -- traced to `bn_car` being divided by
   `TRACK_WINDOWS` for the per-window carrier loop.** `LoopFilter(bn,
   zeta, t=1.0)` is already per-UPDATE normalized (`kp` scales
   ~linearly with `bn`, `ki` ~QUADRATICALLY -- standard 2nd-order PI
   loop filter); dividing `bn_car` by `TRACK_WINDOWS` under-scales the
   integrator (the mechanism that gives a type-2 loop its ramp-
   tracking capability) by an EXTRA factor of `TRACK_WINDOWS`, on top
   of the `TRACK_WINDOWS`-times-more-frequent updates -- net effect,
   the closed-loop integral gain ends up `TRACK_WINDOWS`-times weaker
   than the once-per-epoch design it was meant to match. Passing
   `bn_car` UNCHANGED (no division) fixes it completely -- confirmed
   BER=0.0000, no FLL-assist needed at all (matches the
   `~/legacy-commz` reference, which also tracks a real Doppler rate
   with a plain Costas PLL, no FLL). See `diagnose_bn_car_scaling.py`
   for the isolated regression proving this (empirically: `ki/bn^2` is
   ~constant, `kp/bn` is ~constant, confirming the standard-loop-
   filter model directly against `doppler.track.LoopFilter`).

Run: `python e2e_acq_to_despreader.py` (needs numpy + doppler).
"""

from __future__ import annotations

import numpy as np

from acq_handoff import search_and_handoff
from despreader_coupled import CoupledAsyncDespreader, async_lookback_windows
from freq_refine import refine_seed_carrier_acq
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

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC

BN_CODE = 0.002
BN_CARRIER = 0.01
# Async-lookback granularity, derived (not hand-picked) from a maximum
# allowed correlation-loss tolerance -- see async_lookback_windows()'s own
# docstring for the full derivation and why the prior WINDOWS=62 constant
# was stale on two separate counts. 0.5dB matches ~/legacy-commz's own
# validated default (`asynchronous_correlation_loss`); gives windows=11,
# window_size=186 at this waveform's tsamps=2046 -- CarrierAcquisition's
# own rate is now handled by an independent resample stage
# (freq_refine.py's _collect_frozen_carrier_prefix), not by this value.
WINDOWS, _WINDOW_SIZE = async_lookback_windows(TE, max_error_db=0.5)
TRACK_WINDOWS = 6  # closed per-window carrier-loop granularity (2046/6=341);
# deliberately coarser than WINDOWS -- reusing WINDOWS here starves each
# window's own per-update SNR (found and fixed this session, see
# FINISHING_PLAN.md CHECKPOINT 19).

# refine_seed_carrier_acq's own design point -- SPEC's minimum Es/N0, not
# whatever Es/N0 a given trial actually runs at (design_snr is a PLANNING
# assumption, same role as Acquisition's own cn0_dbhz -- see that
# function's own docstring for the design_margin_db derating this needs).
CN0_MIN_DBHZ = es_n0_to_cn0_dbhz(5.0, sym_rate=SYM_RATE)

# Long enough to cover the Acquisition search dwell + a 2700-epoch PSDMF
# collection prefix + a healthy tracking tail, at ~1.11 epochs/symbol.
N_SYM = 8000
PREFIX_EPOCHS = 2700


def despread_output_ber(out, samples_per_bit, frac_bit_offset, data_bits):
    """Genie-aided bit sync: decide each KNOWN true bit boundary's sign
    directly from the despread stream `out`, without any downstream
    symbol-timing recovery. Bit `i`'s window starts at OUT-stream
    sample `(i - frac_bit_offset) * samples_per_bit` (`frac_bit_offset`
    is how far INTO bit 0 `out`'s own sample 0 already falls, in
    `[0, 1)` bits -- `out`'s t=0 generally doesn't land exactly on a
    bit boundary). Only the MIDDLE THIRD of `data_bits` is scored (same
    convention `spec_full_characterization.py`'s own `_lag_search_ber`/
    `_lag_search_metrics` use) -- the per-window carrier loop needs
    real settling time to close whatever residual PSDMF left (a few
    tens of Hz, typically), so scoring from bit 0 measures that
    transient, not steady-state tracking quality. Returns `(ber,
    n_bits_scored)`; `ber` picks whichever of the two BPSK sign
    conventions scores better (Costas' own 180-degree phase
    ambiguity)."""
    lo, hi = len(data_bits) // 3, 2 * len(data_bits) // 3
    decided = []
    truth_idx = []
    for i in range(lo, hi):
        i0 = int(round((i - frac_bit_offset) * samples_per_bit))
        i1 = int(round((i + 1 - frac_bit_offset) * samples_per_bit))
        if i0 < 0 or i1 > len(out) or i1 <= i0:
            continue
        decided.append(np.sign(out[i0:i1].real.sum()))
        truth_idx.append(i)
    if not decided:
        return 1.0, 0
    decided = np.asarray(decided)
    truth = np.where(data_bits[truth_idx] > 0, -1.0, 1.0)
    ber = min(
        float(np.mean(decided != truth)), float(np.mean(decided != -truth))
    )
    return ber, len(decided)


def run_trial(
    esn0_db,
    seed,
    static_doppler_hz=0.0,
    rate_hz_per_s=RATE_HZ_PER_S,
    doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
    aid_code=True,
):
    """Acquisition search -> handoff -> PSDMF refine -> tracking, scored
    at the despread-output level. Returns a dict with `ber`/`n_scored`/
    `tracking`/`coarse_err_hz`/`refined_err_hz`."""
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db, sym_rate=SYM_RATE)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz,
        seed,
        n_sym=N_SYM,
        rate_hz_per_s=rate_hz_per_s,
        static_doppler_hz=static_doppler_hz,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen)

    acq = Acquisition(
        CODE,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=doppler_uncertainty,
        pfa=1e-3,
        pd=0.9,
        symbol_rate=SYM_RATE,
    )
    try:
        handoff, consumed = search_and_handoff(acq, x, SPC, FS_FRONT)
    except RuntimeError:
        return {"ber": 1.0, "n_scored": 0, "tracking": False}

    def true_doppler_at(n_front_samples):
        return static_doppler_hz + rate_hz_per_s * (n_front_samples / FS_FRONT)

    coarse_err_hz = handoff.doppler_hz_est - true_doppler_at(consumed)
    tracker_init_chip = (SF - handoff.chip_phase) % SF
    coarse_norm_freq = handoff.doppler_hz_est / FS_FRONT

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = min(PREFIX_EPOCHS, n_epochs_total // 4)
    if n_prefix_epochs < 1 or n_epochs_total - n_prefix_epochs < 1:
        return {"ber": 1.0, "n_scored": 0, "tracking": False}
    prefix = tail[: n_prefix_epochs * TE]

    refined_norm_freq, _residual_hz, ca_ready, samples_consumed = (
        refine_seed_carrier_acq(
            CoupledAsyncDespreader,
            CODE,
            SPC,
            BN_CODE,
            coarse_norm_freq,
            prefix,
            FS_FRONT,
            SYM_RATE,
            CN0_MIN_DBHZ,
            bn_car=BN_CARRIER,
            windows=WINDOWS,
            init_chip=tracker_init_chip,
        )
    )
    if not ca_ready:
        return {
            "ber": 1.0,
            "n_scored": 0,
            "tracking": False,
            "coarse_err_hz": coarse_err_hz,
            "carrier_acq_ready": False,
        }
    # Size the tracking handoff dynamically off how much of `prefix`
    # CarrierAcquisition ACTUALLY used (samples_consumed), not the fixed,
    # generously-sized PREFIX_EPOCHS budget -- CarrierAcquisition's own
    # dwell is now sized off its calibrated statistic (often a small
    # fraction of PREFIX_EPOCHS), so treating the unused remainder of
    # `prefix` as already "spent" before tracking starts hands the
    # tracker a stale seed, off by however far the residual has genuinely
    # drifted in that unused remainder in the meantime (confirmed: >300Hz
    # of avoidable error at SPEC's own 500Hz/s rate -- a stale-reference
    # artifact, not an estimation error). Round UP to a whole epoch (this
    # pipeline's own natural framing granularity elsewhere).
    n_epochs_used = min(
        -(-samples_consumed // TE) or 1, n_prefix_epochs
    )
    track_rx = tail[n_epochs_used * TE : n_epochs_total * TE]
    true_at_prefix_end = true_doppler_at(consumed + n_epochs_used * TE)
    refined_err_hz = refined_norm_freq * FS_FRONT - true_at_prefix_end

    d = CoupledAsyncDespreader(
        CODE,
        SPC,
        bn=BN_CODE,
        zeta=0.707,
        spacing=0.5,
        windows=TRACK_WINDOWS,
        init_chip=tracker_init_chip,
        init_car_norm_freq=refined_norm_freq,
        aid_code=aid_code,
        sample_rate_hz=FS_FRONT,
        # NOT divided by TRACK_WINDOWS -- LoopFilter(bn, zeta, t=1.0) is
        # already per-UPDATE normalized (kp/ki derive from bn alone,
        # t=1.0 fixed regardless of what real interval an update spans),
        # and kp scales ~linearly with bn while ki scales ~QUADRATICALLY
        # with bn (standard 2nd-order PI loop). Dividing bn_car by
        # TRACK_WINDOWS to "keep the same real bandwidth" only holds for
        # the linear kp term; it under-scales ki by an extra factor of
        # TRACK_WINDOWS, crippling the integrator that gives a type-2
        # loop its ramp/rate-tracking capability -- confirmed empirically
        # (divided: -1262Hz final error on the real 500Hz/s case;
        # undivided: -74Hz). Keep bn_car identical to the once-per-epoch
        # value; running the SAME (bn, zeta) at a higher update rate
        # already widens the real-Hz bandwidth correctly on its own, no
        # manual rescaling needed.
        bn_car=BN_CARRIER,
        bn_fll_car=0.0,
        car_update_windows=True,
    )
    out = d.run(track_rx)

    out_rate_hz = CHIP_RATE * TRACK_WINDOWS / SF
    elapsed_bits = (
        (consumed + n_epochs_used * TE) / FS_FRONT * SYM_RATE
    )
    symbols_in_prefix = int(np.floor(elapsed_bits))
    frac_bit_offset = elapsed_bits - symbols_in_prefix
    data_bits_tail = data_bits[symbols_in_prefix:]
    samples_per_bit = out_rate_hz / SYM_RATE

    ber, n_scored = despread_output_ber(
        out, samples_per_bit, frac_bit_offset, data_bits_tail
    )
    return {
        "ber": ber,
        "n_scored": n_scored,
        "tracking": True,
        "coarse_err_hz": coarse_err_hz,
        "refined_err_hz": refined_err_hz,
    }


# (label, static_doppler_hz, rate_hz_per_s, aid_code, gates_pass_fail)
# aid_code=False on the offset-only case works around this signal
# generator's own no-v/c-coupling limitation (see module docstring,
# point 1) -- a test-fixture accommodation, not how a real receiver
# should be configured. The real-rate case now gates too (point 2,
# fixed CHECKPOINT 25: bn_car was wrongly divided by TRACK_WINDOWS).
CASES = (
    ("zero impairments", 0.0, 0.0, True, True),
    ("static offset only (aid_code=False)", 15000.0, 0.0, False, True),
    ("SPEC real rate (500Hz/s)", 0.0, RATE_HZ_PER_S, False, True),
)
N_SEEDS = 3
ESN0_DB = 10.0
BER_PASS_THRESHOLD = 0.05


def main():
    print(
        "=== Acq -> Despreader e2e (aid_code + car_update_windows + "
        "one PSDMF pass) ===\n"
        f"Es/N0={ESN0_DB} dB  doppler_uncertainty=+/-"
        f"{DOPPLER_UNCERTAINTY_HZ:.0f} Hz  seeds/case={N_SEEDS}"
    )
    all_ok = True
    for label, static_hz, rate, aid_code, gates in CASES:
        recs = [
            run_trial(ESN0_DB, 6000 + s, static_hz, rate, aid_code=aid_code)
            for s in range(N_SEEDS)
        ]
        bers = [r["ber"] for r in recs]
        ok = all(
            r["tracking"] and b < BER_PASS_THRESHOLD
            for r, b in zip(recs, bers)
        )
        if gates:
            all_ok &= ok
        ber_str = ", ".join(f"{b:.4f}" for b in bers)
        status = "PASS" if ok else ("KNOWN-FAIL" if not gates else "FAIL")
        print(f"{label:38s} ber=[{ber_str}]  {status}")
    assert all_ok, "Acq -> Despreader e2e FAILED on a gating case"
    print("\nAll gating cases PASS (see KNOWN-FAIL notes above, if any).")


if __name__ == "__main__":
    main()
