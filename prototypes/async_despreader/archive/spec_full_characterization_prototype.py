"""Runs `spec_full_characterization.py`'s EXACT SAME combined-SPEC test
(real CCSDS Gold-1023 code, SPEC's own chip_rate/symbol_rate, real
+/-50kHz wideband Acquisition search, 500 Hz/s ramp, Es/N0 swept
{3,5,10,20} dB, same seeds/BER/EVM metrics) against the ORIGINAL,
already-validated Python prototype tracker (`CoupledAsyncDespreader`,
`despreader_coupled.py`) instead of the shipped C `DsssReceiver` --
a direct before/after comparison of this session's task #100 fix
(the carrier data-wiping fix, applied only in the C composition) against
the prototype it was ported from, which never got that fix.

Everything upstream and downstream of the tracker itself is reused
verbatim, not reimplemented, per this project's own discipline:
  - Signal generation, Es/N0 sweep, seeds, theory/EVM metrics -- imported
    directly from `spec_full_characterization.py`.
  - Acquisition handoff (`handoff_from_hit`/`search_and_handoff`, and the
    `tracker_init_chip = (SF - chip_phase) % SF` mirror-image fix this
    prototype's OWN `init_chip` convention needs -- a real, previously
    found bug, `acq_handoff.py`'s own module docstring) -- imported
    directly from `acq_handoff.py`.
  - The downstream RateConverter -> MpskReceiver demod stage --
    constructed with the IDENTICAL parameters `dsss_receiver_core.c`'s
    own `_build_chain()` uses (same `bn_carrier`/`bn_timing`/`zeta`/
    `acq_to_track`/`lock_thresh`/`warmup_syms`, same `partial_rate`/
    `target_rate` formulas), mirroring `async_dsss_receiver_demo.py`'s
    own `_new_chain()` helper -- so any BER/EVM difference measured here
    is attributable to the TRACKER (CoupledAsyncDespreader vs the C
    Costas+Dll composition), not to a different downstream demod.

Architecture-matching choices, to keep this an apples-to-apples
comparison rather than a different receiver entirely:
  - `bn=0.002` (the code loop) matches `Dll`'s own hardcoded bn in
    `dsss_receiver_create()`, not `CoupledAsyncDespreader`'s own
    example-script default.
  - `bn_car=DSSS_RX_BN_CARRIER`, `bn_fll_car=DSSS_RX_BN_FLL` match the C
    object's own hardcoded carrier-loop constants exactly.
  - `aid_code=False` -- the shipped C `DsssReceiver` does NOT feed a
    carrier-rate aiding term into `Dll` (see `dsss_receiver_core.c`'s own
    `_track_carrier_dll`: costas_wipeoff -> dll_steps -> costas_update
    only, no rate-aiding path back into the code loop), so the prototype
    is run the same way for a fair comparison.
  - `windows=6` -- `CoupledAsyncDespreader.windows` requires
    `tsamps % windows == 0` (tsamps = sf*spc = 2046 = 2*3*11*31); 4
    (DsssReceiver's own `segments` default) does not divide 2046, so 6
    (2046/6=341) is the closest exact divisor, matching `acq_handoff.py`'s
    own established choice at this exact code/rate.

This is the ONE thing this prototype's own carrier discriminator does
differently from the just-fixed C composition: `despreader_coupled.py`'s
`prompt = output.mean()` is a bare coherent average over the WHOLE epoch
(all `windows` sub-chunks pooled with no data-wiping at all) -- the
straightforward extension of "one coherent full-epoch prompt" to a
`windows`-chunked epoch, never revisited after `windows>1` needed
task #52's own async-lookback fix for the CODE loop. It was only ever
validated at conceptually `windows=1` framing (the module docstring's
own "carrier tracking doesn't need the code loop's async-data lookback
machinery" reasoning) -- exactly the same gap task #100 found and fixed
in the C port.

Run: `python spec_full_characterization_prototype.py` (needs numpy +
doppler; takes a few minutes -- N_SEEDS acquisition searches per Es/N0
point, same cost profile as `spec_full_characterization.py`).
"""

from __future__ import annotations

import numpy as np

from acq_handoff import search_and_handoff
from despreader_coupled import CoupledAsyncDespreader
from freq_refine import refine_seed_matched
from spec_full_characterization import (
    CODE,
    CHIP_RATE,
    DOPPLER_UNCERTAINTY_HZ,
    ESN0_LIST_DB,
    FS_GEN,
    N_SEEDS,
    N_SYM,
    PERIODS_PER_SYMBOL,
    RATE_HZ_PER_S,
    SF,
    SPC,
    SYM_RATE,
    _lag_search_metrics,
    _theory_ber_bpsk,
    es_n0_to_cn0_dbhz,
    make_ramp_signal,
)

from doppler.dsss import Acquisition
from doppler.resample import RateConverter
from doppler.track import MpskReceiver

# The C DsssReceiver's own hardcoded carrier-loop constants
# (dsss_receiver_core.h) -- reused here verbatim for a fair comparison.
BN_CARRIER = 0.01
BN_FLL = 0.03
BN_CODE = 0.002  # matches Dll's own hardcoded bn in dsss_receiver_create()

WINDOWS = 6  # closest exact divisor of tsamps=2046 to DsssReceiver's
# own segments=4 default (2046 = 2*3*11*31, not divisible by 4) -- used
# ONLY by run_trial_prototype/_run_sweep's C-comparison tests, where
# matching DsssReceiver's own segments=4 grid matters more than the
# Nyquist margin below (that comparison never needed a real +/-50kHz
# static offset -- doppler_uncertainty was 0 or the offset was 0 in
# every sweep that used it).

# Nyquist floor for the despreader's own output rate (the "windows"-rate
# stream both the carrier loop's per-epoch discriminator effectively
# runs against and the PSDMF estimator/downstream RateConverter consume):
# chip_rate*windows/sf >= 2*(symbol_rate + max_doppler_hz), or the
# stream can't unambiguously represent a residual carrier anywhere near
# the full search uncertainty without aliasing. At this waveform's own
# numbers (chip_rate=3.069e6, sf=1023, symbol_rate=2700,
# max_doppler=DOPPLER_UNCERTAINTY_HZ=50000): windows >= sf*2*(2700+
# 50000)/chip_rate ~= 35.1 -- WINDOWS=6 (rate ~18kHz) is ~6x under this,
# and the carrier loop's own per-EPOCH discriminator rate (chip_rate/sf
# ~=3kHz) is ~35x under it. 62 is the smallest exact divisor of
# tsamps=2046=2*3*11*31 clearing the >=35.1 requirement (2046/62=33,
# rate = chip_rate*62/sf ~= 186kHz), used by the feed-forward
# (Acquisition+PSDMF) verification functions below, which DO need to
# handle a real, uncorrected-magnitude residual up to the full
# +/-50kHz uncertainty.
WINDOWS_HIRES = 62

MPSK_SPS = 8
MPSK_N = 4  # _derive_n(8) in dsss_receiver_core.c

TE = SF * SPC


def run_trial_prototype(
    esn0_db,
    seed,
    rate_hz_per_s=RATE_HZ_PER_S,
    doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
    bn_fll_car=BN_FLL,
    n_sym=N_SYM,
    bn_car=BN_CARRIER,
):
    """`rate_hz_per_s`/`doppler_uncertainty` default to SPEC's own real
    values (every existing call site's behavior unchanged); pass 0.0 for
    either/both for a zero-impairments control run -- no Doppler RATE
    ramp AND/OR no wideband search burden (Acquisition already validates
    `doppler_uncertainty >= 0.0`, acq_core.c, so 0.0 is a legal "single-
    bin, no-search" grid, not a degenerate case). `bn_fll_car` defaults
    to the C object's own BN_FLL; pass 0.0 to disable FLL-assist (a pure
    PLL, the module's own byte-identical-to-before-the-feature
    convention) -- isolates whether the FLL cross-product discriminator
    itself is contributing instability at low Es/N0, independent of
    whether there is an actual Doppler rate to track (task #99's other
    not-yet-investigated candidate). `n_sym` defaults to N_SYM
    (~0.74s at this waveform's own FS_GEN/DATA_SPS) -- pass a larger
    value for a longer observation window, needed to actually reach the
    ~117-120Hz/s pure-PLL cliff this project's own earlier
    characterization found (an 8000-epoch, ~4s trace) at a real
    500Hz/s ramp rate; N_SYM's own short default window never lets the
    true residual drift past ~370Hz, nowhere near enough to expose that
    cliff if FLL-assist is disabled. `bn_car` defaults to the C
    object's own BN_CARRIER (0.01); pass a wider value (e.g. 0.02) to
    test whether widening the bare-PLL loop bandwidth shifts the
    4-5dB pull-in cliff task #99 bracketed -- a wider bandwidth trades
    more pull-in range for more steady-state noise/jitter."""
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz, seed, n_sym=n_sym, rate_hz_per_s=rate_hz_per_s
    )

    fs_front = CHIP_RATE * SPC
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)

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

    record = {"esn0_db": esn0_db, "seed": seed, "tracking": 0, "n_syms": 0}
    try:
        handoff, consumed = search_and_handoff(acq, x, SPC, fs_front)
    except RuntimeError:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record

    # This prototype's own init_chip convention is the MIRROR IMAGE of
    # Acquisition's code_phase (acq_handoff.py's own found-the-hard-way
    # bug) -- must apply the same flip here or the tracker starts at the
    # wrong code phase regardless of tracking quality.
    tracker_init_chip = (SF - handoff.chip_phase) % SF

    d = CoupledAsyncDespreader(
        CODE,
        SPC,
        bn=BN_CODE,
        zeta=0.707,
        spacing=0.5,
        windows=WINDOWS,
        init_chip=tracker_init_chip,
        init_car_norm_freq=handoff.doppler_hz_est / fs_front,
        aid_code=False,
        sample_rate_hz=fs_front,
        bn_car=bn_car,
        bn_fll_car=bn_fll_car,
    )

    tail = x[consumed:]
    run_len = (len(tail) // TE) * TE
    if run_len < TE:
        record["tracking"] = 1
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record
    out = d.run(tail[:run_len])
    record["tracking"] = 1

    # Downstream RateConverter -> MpskReceiver, parameters matching
    # dsss_receiver_core.c's own _build_chain() exactly (see module
    # docstring) -- so any BER/EVM delta is attributable to the tracker.
    partial_rate = CHIP_RATE * WINDOWS / SF
    target_rate = MPSK_SPS * SYM_RATE
    rc = RateConverter(rate=target_rate / partial_rate)
    rc_out = rc.execute(out.astype(np.complex64))

    rx = MpskReceiver(
        m=2,
        sps=MPSK_SPS,
        n=MPSK_N,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        zeta=0.707,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=handoff.doppler_hz_est / target_rate,
        warmup_syms=30,
        differential=0,
    )
    syms = rx.steps(rc_out)

    record["n_syms"] = len(syms)
    if len(syms) < 10:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record

    record["ber"], record["evm_db"] = _lag_search_metrics(syms, data_bits)
    return record


def run_trial_prototype_psd_seeded(
    esn0_db,
    seed,
    rate_hz_per_s=RATE_HZ_PER_S,
    doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
    n_sym=N_SYM,
    bn_car=BN_CARRIER,
    prefix_fraction=0.5,
    windows=WINDOWS_HIRES,
):
    """FLL STRIPPED (`bn_fll_car=0.0` unconditionally -- a pure PLL,
    matching the module docstring's own architecture note: pre-despread
    tracking only needs to hold a SMALL residual once well-seeded, it
    doesn't need FLL's wide-but-noisy pull-in range at all if the seed
    is already accurate).

    Instead of relying on the closed loop to pull in from Acquisition's
    own coarse seed directly (the previously-characterized 4-5dB
    cliff), this collects a FROZEN-CARRIER prefix (the first
    `prefix_fraction` of the available signal, `refine_seed_matched`'s
    own established collection pattern -- code loop tracks, carrier
    NCO holds at the coarse seed) and estimates the residual carrier
    with `estimate_residual_freq_matched` -- the quasi-ML, no-squaring-
    loss PSD-correlation estimator (see `freq_refine.py`). A FRESH
    tracker is then constructed seeded at the REFINED frequency and
    runs the remainder of the signal for actual tracking/demod -- "it
    can then track wherever it goes since it is applied pre-despread"
    (a pure PLL only needs to hold whatever small residual/rate is left
    once accurately seeded, exactly like the already-validated 5dB+
    bare-PLL results).

    Because a sizeable prefix is consumed purely for the frequency
    estimate (non-coherent SNR accumulation, not tracked/demodulated),
    `data_bits` is sliced to skip the symbols that fell inside that
    prefix before the usual lag search -- the prefix is typically far
    more than the +-50 symbol lag search's own quantization-slop
    margin, unlike `run_trial_prototype`'s implicit (and much shorter)
    Acquisition-search-only prefix.
    """
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz, seed, n_sym=n_sym, rate_hz_per_s=rate_hz_per_s
    )

    fs_front = CHIP_RATE * SPC
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)

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

    record = {"esn0_db": esn0_db, "seed": seed, "tracking": 0, "n_syms": 0}
    try:
        handoff, consumed = search_and_handoff(acq, x, SPC, fs_front)
    except RuntimeError:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record

    tracker_init_chip = (SF - handoff.chip_phase) % SF
    coarse_norm_freq = handoff.doppler_hz_est / fs_front

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = int(n_epochs_total * prefix_fraction)
    if n_prefix_epochs < 1 or n_epochs_total - n_prefix_epochs < 1:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record
    prefix = tail[: n_prefix_epochs * TE]
    track_rx = tail[n_prefix_epochs * TE : n_epochs_total * TE]

    refined_norm_freq, residual_hz = refine_seed_matched(
        CoupledAsyncDespreader,
        CODE,
        SPC,
        BN_CODE,
        coarse_norm_freq,
        prefix,
        fs_front,
        SYM_RATE,
        n_fft=64,
        zero_pad=4,
        interp=True,
        bn_car=bn_car,
        windows=windows,
        init_chip=tracker_init_chip,
    )
    record["residual_hz"] = residual_hz

    d = CoupledAsyncDespreader(
        CODE,
        SPC,
        bn=BN_CODE,
        zeta=0.707,
        spacing=0.5,
        windows=windows,
        init_chip=tracker_init_chip,
        init_car_norm_freq=refined_norm_freq,
        aid_code=False,
        sample_rate_hz=fs_front,
        bn_car=bn_car,
        bn_fll_car=0.0,  # FLL stripped -- see docstring
    )
    out = d.run(track_rx)
    record["tracking"] = 1

    # Downstream RateConverter -> MpskReceiver, seeded from the SAME
    # refined estimate (strictly better information than the coarse
    # Acquisition-only seed `run_trial_prototype` uses here).
    partial_rate = CHIP_RATE * windows / SF
    target_rate = MPSK_SPS * SYM_RATE
    rc = RateConverter(rate=target_rate / partial_rate)
    rc_out = rc.execute(out.astype(np.complex64))

    refined_hz = refined_norm_freq * fs_front
    rx = MpskReceiver(
        m=2,
        sps=MPSK_SPS,
        n=MPSK_N,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        zeta=0.707,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=refined_hz / target_rate,
        warmup_syms=30,
        differential=0,
    )
    syms = rx.steps(rc_out)

    record["n_syms"] = len(syms)
    if len(syms) < 10:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record

    # The prefix consumed roughly this many DATA SYMBOLS (periods/
    # symbol is generally non-integer -- see PERIODS_PER_SYMBOL) --
    # skip them so the lag search's own +-50 margin only has to absorb
    # rounding slop, not the whole prefix length.
    symbols_in_prefix = int(round(n_prefix_epochs / PERIODS_PER_SYMBOL))
    data_bits_tail = data_bits[symbols_in_prefix:]

    record["ber"], record["evm_db"] = _lag_search_metrics(
        syms, data_bits_tail
    )
    return record


def verify_feedforward_seed(
    esn0_db,
    seed,
    static_doppler_hz,
    rate_hz_per_s=RATE_HZ_PER_S,
    doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
    n_sym=N_SYM,
    bn_car=BN_CARRIER,
    prefix_fraction=0.5,
    windows=WINDOWS_HIRES,
    track_windows=WINDOWS,
):
    """Verifies the FULL feed-forward frequency-acquisition chain --
    Acquisition's real +/-50kHz wideband search (`doppler_uncertainty`)
    handing off to the PSD-matched-filter (PSDMF) refinement
    (`estimate_residual_freq_matched`) -- against a GENUINELY nonzero
    true residual (`static_doppler_hz`, injected via
    `make_ramp_signal`'s new parameter) plus the real 500Hz/s rate, at
    a fixed Es/N0 (intended for 5dB, where task #99's own
    characterization already established a bare Costas PLL locks
    reliably once well-seeded). Unlike `run_trial_prototype_psd_seeded`
    (which used `doppler_uncertainty=0`/`static_doppler_hz=0` --
    trivial cases where Acquisition's own coarse estimate is already
    near-perfect), this is the real end-to-end architecture SPEC.md
    describes: Acquisition resolves the coarse +/-50kHz uncertainty
    down to one ~3kHz-wide bin, PSDMF refines THAT residual further
    (a quasi-ML estimate, no squaring loss), and the tracking pass now
    mirrors the `~/legacy-commz` reference's own nested two-rate loop
    (CHECKPOINT 16, `FINISHING_PLAN.md`/project memory): `aid_code=True`
    (the item-1 v/c-coupling fix, aiding the code loop from the FULL
    current carrier estimate) and `car_update_windows=True` (the item-2
    per-window carrier discriminator, replacing the once-per-epoch
    update whose own Nyquist margin -- not the despread output
    stream's -- was the real gap). FLL-assist stays off
    (`bn_fll_car=0.0`) -- already shown sufficient to track the real
    500Hz/s rate once well-seeded, see FINISHING_PLAN.md's long-window
    findings.

    `windows` (default `WINDOWS_HIRES`) is used ONLY for the PSDMF
    collection pass (`refine_seed_matched`) -- it needs the wide
    Nyquist margin to represent a still-uncorrected residual anywhere
    in the full search range. `track_windows` (default `WINDOWS`, a
    much coarser divisor) sizes the SEPARATE, FINAL tracking-pass
    tracker instead: once PSDMF has already narrowed the residual to a
    small value, the closed per-window carrier loop needs enough
    samples per window for a reasonable per-update SNR, not the
    collection stage's wide-residual Nyquist margin -- reusing
    `WINDOWS_HIRES` here was tried first and measured to fail
    (each window's coherent sum too short for the discriminator to
    move at all; see FINISHING_PLAN.md). `bn_car` is renormalized to
    `track_windows`' own per-window update rate (dividing by
    `track_windows` alone preserves the SAME absolute loop bandwidth
    `bn_car` gives the once-per-epoch architecture elsewhere in this
    file, since `bn_car` is epoch-rate-normalized per
    `CoupledAsyncDespreader`'s own convention, not symbol-rate
    -- mirroring the reference's own `_loop_bandwidth =
    loop_bandwidth / osr` at whatever granularity the closed loop
    actually runs at).

    Returns coarse (Acquisition-only) and refined (Acquisition+PSDMF)
    frequency errors against the TRUE instantaneous residual at the
    relevant moment (accounting for how far the 500Hz/s ramp has
    already moved it by the time of the hit / end of the PSDMF prefix
    -- a fixed comparison against `static_doppler_hz` alone would be
    wrong once the ramp has run for any real duration), plus the
    resulting BER/EVM -- so "close enough for Costas to lock" is a
    directly checkable, quantitative claim, not just a pass/fail.
    """
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz,
        seed,
        n_sym=n_sym,
        rate_hz_per_s=rate_hz_per_s,
        static_doppler_hz=static_doppler_hz,
    )

    fs_front = CHIP_RATE * SPC
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)

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

    record = {
        "esn0_db": esn0_db,
        "seed": seed,
        "static_doppler_hz": static_doppler_hz,
        "tracking": 0,
        "n_syms": 0,
    }
    try:
        handoff, consumed = search_and_handoff(acq, x, SPC, fs_front)
    except RuntimeError:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        record["coarse_err_hz"] = float("nan")
        record["refined_err_hz"] = float("nan")
        return record

    # True instantaneous residual at time t (seconds from t=0 of the
    # RAMP SIGNAL ITSELF, matching make_ramp_signal's own phase
    # convention) -- the ramp keeps moving the truth throughout, so
    # "true" is only meaningful at a specific moment, not a constant.
    def true_doppler_at(n_front_samples):
        t_s = n_front_samples / fs_front
        return static_doppler_hz + rate_hz_per_s * t_s

    true_at_hit = true_doppler_at(consumed)
    record["coarse_err_hz"] = handoff.doppler_hz_est - true_at_hit

    tracker_init_chip = (SF - handoff.chip_phase) % SF
    coarse_norm_freq = handoff.doppler_hz_est / fs_front

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = int(n_epochs_total * prefix_fraction)
    if n_prefix_epochs < 1 or n_epochs_total - n_prefix_epochs < 1:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        record["refined_err_hz"] = float("nan")
        return record
    prefix = tail[: n_prefix_epochs * TE]
    track_rx = tail[n_prefix_epochs * TE : n_epochs_total * TE]

    refined_norm_freq, residual_hz = refine_seed_matched(
        CoupledAsyncDespreader,
        CODE,
        SPC,
        BN_CODE,
        coarse_norm_freq,
        prefix,
        fs_front,
        SYM_RATE,
        n_fft=64,
        zero_pad=4,
        interp=True,
        bn_car=bn_car,
        windows=windows,
        init_chip=tracker_init_chip,
    )
    true_at_prefix_end = true_doppler_at(consumed + n_prefix_epochs * TE)
    refined_hz = refined_norm_freq * fs_front
    record["refined_err_hz"] = refined_hz - true_at_prefix_end

    # Per-window carrier loop (item 2): bn_car is epoch-rate-normalized
    # everywhere else in this file (CoupledAsyncDespreader's own "same
    # once-per-epoch-update convention as bn"), so preserving the SAME
    # absolute loop bandwidth at track_windows-per-epoch granularity is
    # a plain bn_car/track_windows -- see docstring for why this is a
    # SEPARATE, coarser windows count than the PSDMF collection's own
    # WINDOWS_HIRES.
    bn_car_per_window = bn_car / track_windows

    d = CoupledAsyncDespreader(
        CODE,
        SPC,
        bn=BN_CODE,
        zeta=0.707,
        spacing=0.5,
        windows=track_windows,
        init_chip=tracker_init_chip,
        init_car_norm_freq=refined_norm_freq,
        aid_code=True,
        sample_rate_hz=fs_front,
        bn_car=bn_car_per_window,
        bn_fll_car=0.0,  # FLL stripped
        car_update_windows=True,
    )
    out = d.run(track_rx)
    record["tracking"] = 1

    partial_rate = CHIP_RATE * track_windows / SF
    target_rate = MPSK_SPS * SYM_RATE
    rc = RateConverter(rate=target_rate / partial_rate)
    rc_out = rc.execute(out.astype(np.complex64))

    rx = MpskReceiver(
        m=2,
        sps=MPSK_SPS,
        n=MPSK_N,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        zeta=0.707,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=refined_hz / target_rate,
        warmup_syms=30,
        differential=0,
    )
    syms = rx.steps(rc_out)

    record["n_syms"] = len(syms)
    if len(syms) < 10:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record

    symbols_in_prefix = int(round(n_prefix_epochs / PERIODS_PER_SYMBOL))
    data_bits_tail = data_bits[symbols_in_prefix:]
    record["ber"], record["evm_db"] = _lag_search_metrics(
        syms, data_bits_tail
    )
    return record


def _run_sweep(
    label, rate_hz_per_s, doppler_uncertainty, bn_fll_car=BN_FLL, n_sym=N_SYM
):
    print(
        f"=== SPEC full combined characterization -- PROTOTYPE "
        f"(CoupledAsyncDespreader) -- {label} ===\n"
        f"chip_rate={CHIP_RATE:.4e} Hz  symbol_rate={SYM_RATE} Hz  "
        f"periods/symbol={PERIODS_PER_SYMBOL:.4f} (async)\n"
        f"doppler_uncertainty=+/-{doppler_uncertainty:.0f} Hz  "
        f"rate={rate_hz_per_s:.0f} Hz/s  seeds/point={N_SEEDS}  n_sym={n_sym}\n"
        f"bn={BN_CODE}  bn_car={BN_CARRIER}  bn_fll_car={bn_fll_car}  "
        f"windows={WINDOWS}  aid_code=False (matches the C composition's "
        f"own lack of carrier->code rate aiding)"
    )
    for esn0_db in ESN0_LIST_DB:
        cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)
        recs = [
            run_trial_prototype(
                esn0_db,
                5000 + s,
                rate_hz_per_s,
                doppler_uncertainty,
                bn_fll_car,
                n_sym,
            )
            for s in range(N_SEEDS)
        ]
        n_tracking = sum(r["tracking"] for r in recs)
        bers = [r["ber"] for r in recs]
        evms = [r["evm_db"] for r in recs]
        theory = _theory_ber_bpsk(esn0_db)
        theory_evm_db = -esn0_db
        print(
            f"\nEs/N0={esn0_db:5.1f} dB  (cn0_dbhz={cn0_dbhz:.2f})  "
            f"tracking={n_tracking}/{N_SEEDS}\n"
            f"  ber=[{', '.join(f'{b:.4f}' for b in bers)}]  "
            f"theory_ber={theory:.3e}\n"
            f"  evm_db=[{', '.join(f'{e:.2f}' for e in evms)}]  "
            f"theory_evm_db={theory_evm_db:.2f}"
        )


def main():
    _run_sweep("SPEC impairments", RATE_HZ_PER_S, DOPPLER_UNCERTAINTY_HZ)


def main_zero_impairments():
    """Control run: rate_hz_per_s=0.0 (no Doppler ramp) AND
    doppler_uncertainty=0.0 (no wideband search burden -- a single-bin
    grid) -- isolates whether the persisting 5dB/10dB variability is
    driven by the Doppler dynamics/search themselves or by something
    else in the tracker/demod chain. `data_bits`/code/Es-N0 sweep are
    otherwise identical to the impaired run above."""
    _run_sweep("ZERO IMPAIRMENTS (rate=0, doppler_uncertainty=0)", 0.0, 0.0)


def main_zero_impairments_fll_off():
    """Same zero-impairments control as above (rate=0,
    doppler_uncertainty=0 -- no Doppler rate to track and no wideband
    search burden either), PLUS `bn_fll_car=0.0` (FLL-assist disabled,
    a pure PLL) -- isolates whether task #99's OTHER not-yet-
    investigated candidate (the FLL cross-product discriminator itself
    injecting instability at low Es/N0, independent of whether there is
    an actual rate to correct) explains any of the persisting 3dB/5dB
    failure, now that the Doppler-dynamics explanation has already been
    refuted by the plain zero-impairments run above."""
    _run_sweep(
        "ZERO IMPAIRMENTS, FLL OFF (rate=0, doppler_uncertainty=0, "
        "bn_fll_car=0)",
        0.0,
        0.0,
        bn_fll_car=0.0,
    )


if __name__ == "__main__":
    main()
    print()
    main_zero_impairments()
    print()
    main_zero_impairments_fll_off()
