"""Isolates the ONE remaining variable between the C `AsyncDsssReceiver`
refine stage and Python's `freq_refine.refine_seed_carrier_acq()` own
collection pass on case 1 (static +15kHz offset): the collection Dll's
own `segments` (C uses `segments=1`, a coherent per-epoch integrate-and-
dump -- forced via `refine_max_error_db=100.0`, see the earlier session's
own finding that `dll_lookback_segments()`'s multi-segment reconstruction
corrupted `CarrierAcquisition`'s estimate at NEAR-ZERO Doppler; Python
uses `windows=11`, `despreader_coupled.async_lookback_windows()`'s own
derived value).

`case1_stage_diagnostics.py` found the refined estimate error is +15.3 Hz
in Python vs -360.5 Hz in C on this SAME large-offset case -- the
OPPOSITE ranking from the earlier near-zero-Doppler finding (there,
segments=1 measured BETTER than segments=11). This script holds
EVERYTHING else fixed (the literal same C `Dll`/`RateConverter`/
`CarrierAcquisition` engines, same raw prefix samples, same frozen-
carrier derotation, same CarrierAcquisition sizing) and varies ONLY
`segments`, to confirm or refute that the segments choice alone explains
the gap rather than some other difference between the two collection
pipelines (chip-phase convention, resample call order, etc).

Run: `python refine_stage_ab.py` (needs numpy + doppler).
"""

from __future__ import annotations

import numpy as np

from acq_handoff import search_and_handoff
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

from doppler.acquire import CarrierAcquisition
from doppler.dsss import Acquisition
from doppler.resample import RateConverter
from doppler.track import Dll

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
ESN0_DB = 10.0
STATIC_DOPPLER_HZ = 15000.0
SEED = 8001
PREFIX_EPOCHS = 2700

REFINE_SAMPLES_PER_SYMBOL = 4
REFINE_DESIGN_MARGIN_DB = 14.0
REFINE_N_FFT = 64
REFINE_ZERO_PAD = 8
DLL_BN = 0.002  # ASYNC_DSSS_RX_DLL_BN, matches async_dsss_receiver_core.h


def collect_and_refine(prefix, coarse_norm_freq, chip_phase, cn0_dbhz,
                        segments, design_margin_db=REFINE_DESIGN_MARGIN_DB):
    """Frozen-carrier derotate (a static complex exponential -- the exact
    equivalent of `costas_wipeoff` on a NEVER-UPDATED `costas_state_t`,
    since freeze_carrier means the NCO free-runs at its seed rate with no
    correction) -> real C `Dll(segments=...)` -> real C `RateConverter`
    -> real C `CarrierAcquisition`, mirroring `_build_refine_chain`/
    `_refine_period` (`async_dsss_receiver_core.c`) field-for-field
    except for `segments` itself."""
    n = np.arange(len(prefix))
    wiped = (prefix * np.exp(-2j * np.pi * coarse_norm_freq * n)).astype(
        np.complex64
    )

    dll = Dll(
        CODE, sps=SPC, init_chip=chip_phase, bn=DLL_BN, zeta=0.707,
        spacing=0.5, segments=segments,
    )
    out0 = dll.steps(wiped)
    # NOT sign-aligned. dll_core.h's own doc on `segments` draws the real
    # line: segments=1 is "a coherent full-epoch integrate-and-dump";
    # segments>1 is explicitly non-coherent (a power-only discriminator/
    # lock detector, never meant to preserve phase relationships between
    # partials). CarrierAcquisition needs coherent samples to FFT -- no
    # post-hoc sign-alignment on segments>1's own output can retrofit
    # that. A version of this probe tried exactly that fix and it
    # empirically improved every segments value's detection a lot, but
    # that was forcing coherence onto a primitive whose non-coherent mode
    # was never designed to have any -- segments>1 should not be used for
    # this collection at all, not patched into working (see
    # async_dsss_receiver_core.c's own _refine_period() comment).

    partial_rate = CHIP_RATE * segments / SF
    target_rate = REFINE_SAMPLES_PER_SYMBOL * SYM_RATE
    out0 = RateConverter(rate=target_rate / partial_rate).execute(
        out0.astype(np.complex64)
    )

    effective_cn0_dbhz = cn0_dbhz - design_margin_db
    design_snr = float(
        np.sqrt(10.0 ** (effective_cn0_dbhz / 10.0) / target_rate)
    )
    resolution_hz = target_rate / REFINE_N_FFT
    ca = CarrierAcquisition(
        np.array([], dtype=np.float32), target_rate, SYM_RATE,
        resolution_hz=resolution_hz, zero_pad=REFINE_ZERO_PAD, pfa=1e-3,
        pd=0.9, design_snr=design_snr, sequential=False,
        max_n_blocks=100000,
    )
    ca.steps(out0.astype(np.complex64))
    return {
        "ready": ca.ready,
        "residual_hz": ca.residual_hz if ca.ready else float("nan"),
        "n_blocks": ca.n_blocks if hasattr(ca, "n_blocks") else None,
    }


def main():
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, _data_bits = make_ramp_signal(
        cn0_dbhz, SEED, n_sym=8000, rate_hz_per_s=0.0,
        static_doppler_hz=STATIC_DOPPLER_HZ,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen)

    acq = Acquisition(
        CODE, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ, pfa=1e-3, pd=0.9,
        symbol_rate=SYM_RATE,
    )
    handoff, consumed = search_and_handoff(acq, x, SPC, FS_FRONT)
    coarse_norm_freq = handoff.doppler_hz_est / FS_FRONT
    print(
        f"Acquisition hit: doppler_hz_est={handoff.doppler_hz_est:.1f} "
        f"(true={STATIC_DOPPLER_HZ:.1f}, err={handoff.doppler_hz_est - STATIC_DOPPLER_HZ:+.1f})  "
        f"chip_phase={handoff.chip_phase:.3f}"
    )

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = min(PREFIX_EPOCHS, n_epochs_total // 4)
    prefix = tail[: n_prefix_epochs * TE].astype(np.complex64)
    print(f"prefix: {n_prefix_epochs} epochs ({len(prefix)} raw samples)\n")

    # dll_core.c's own native chip-phase convention (Acquisition's
    # code_phase/spc directly, NOT CoupledAsyncDespreader's own mirror-
    # image workaround -- acq_handoff.py's own note: that mirror is
    # specific to the Python prototype's tracker, not the real C Dll).
    # Every exact divisor of tsamps=2046=2*3*11*31 that dll_lookback_
    # segments() could plausibly pick, swept to characterize the trend.
    for segments in (1, 2, 3, 6, 11, 22, 31, 62):
        r = collect_and_refine(
            prefix, coarse_norm_freq, handoff.chip_phase, cn0_dbhz,
            segments,
        )
        if r["ready"]:
            refined_hz = handoff.doppler_hz_est + r["residual_hz"]
            err = refined_hz - STATIC_DOPPLER_HZ
            print(
                f"segments={segments:3d}: ready=True   residual_hz={r['residual_hz']:+8.1f}  "
                f"refined={refined_hz:9.1f}  err={err:+8.1f} Hz  (n_blocks={r['n_blocks']})"
            )
        else:
            print(f"segments={segments:3d}: ready=False (never fired)  (n_blocks={r['n_blocks']})")

    # Dwell/variance sweep -- at BOTH segments=1 and segments=11 (the
    # coherent best-window-search mode, not "non-coherent": AsyncDsssReceiver
    # handles genuinely async data BY finding the best-correlating window
    # coherently, which is exactly what dll_core.c's segments>1 lookback
    # does -- an earlier reading of dll_core.h's own "tracks the code
    # non-coherently ACROSS partials" phrase as disqualifying segments>1
    # entirely was wrong; that describes the CODE DISCRIMINATOR's own
    # combination of partials, not the window-selection search itself).
    # segments=1 only ever ran at the default margin_db=14.0 (n_blocks=3)
    # before this -- never swept for dwell.
    print("\n--- dwell sweep (design_margin_db -> more blocks), no sign-align ---")
    for segments in (1, 11):
        for margin_db in (14.0, 20.0, 26.0, 32.0, 38.0, 44.0):
            r = collect_and_refine(
                prefix, coarse_norm_freq, handoff.chip_phase, cn0_dbhz,
                segments, design_margin_db=margin_db,
            )
            if r["ready"]:
                refined_hz = handoff.doppler_hz_est + r["residual_hz"]
                err = refined_hz - STATIC_DOPPLER_HZ
                print(
                    f"segments={segments:3d}  margin_db={margin_db:5.1f}: ready=True   "
                    f"err={err:+8.1f} Hz  (n_blocks={r['n_blocks']})"
                )
            else:
                print(
                    f"segments={segments:3d}  margin_db={margin_db:5.1f}: ready=False  "
                    f"(n_blocks={r['n_blocks']})"
                )


if __name__ == "__main__":
    main()
