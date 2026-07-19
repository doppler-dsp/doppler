"""A/B validation for task #111 (`FINISHING_PLAN.md`): the new jm
C-backed `doppler.acquire.CarrierAcquisition` (a PSDMF residual-carrier
frequency estimator composing `psd_core`/`detector_core`/
`detection_core`) against the already-validated Python prototype
function it replaces, `freq_refine.estimate_residual_freq_matched`
(via `refine_seed_matched`'s own collection pass,
`_collect_frozen_carrier_prefix`).

Both estimators are fed the IDENTICAL despread, window-rate PSDMF
collection prefix -- the real Acquisition -> handoff -> frozen-carrier
collection pipeline `e2e_acq_to_despreader.py`/
`diagnose_bn_car_scaling.py` already use, at `SPEC.md`'s real
+/-50kHz operating point.

**Known open item, NOT a bug in the C object itself** (see
`FINISHING_PLAN.md`'s "Acq -> Despreader cleanup" section and this
session's project memory): `detection_core`'s `det_threshold_noncoherent`/
`det_n_noncoh` were derived for classic complex-correlator (Rayleigh
under noise / Rician under signal) detection statistics -- the same
model `Acquisition`'s own auto-config uses. Reusing them here to gate a
*power-spectrum-vs-known-PSD-template* correlation (a different,
non-negative, non-Gaussian statistical regime) has NOT been verified to
carry over cleanly: a single look can cross the CFAR test confidently
(real signal, not noise) while its own sub-bin peak estimate is still
noisy from the random data modulation's own realization -- only
non-coherent averaging over many looks smooths the measured shape
toward the analytic sinc^2 template, and how many looks that actually
takes doesn't match what `det_n_noncoh`'s classic-correlator formula
predicts. This script reports the comparison either way rather than
asserting pass/fail, so it stays useful as a diagnostic without
pretending the calibration question is closed.

Run: `python carrier_acq_c_vs_python.py <case_index>` (needs numpy +
doppler; ONE case per process -- see feedback_wsl_memory_guard_large_arrays
memory on why this script never loops multiple cases in one process).
"""

from __future__ import annotations

import numpy as np

from acq_handoff import search_and_handoff
from despreader_coupled import CoupledAsyncDespreader
from freq_refine import _collect_frozen_carrier_prefix, estimate_residual_freq_matched
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

from doppler.acquire import CarrierAcquisition
from doppler.dsss import Acquisition
from doppler.resample import RateConverter

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
BN_CODE = 0.002
BN_CARRIER = 0.01
WINDOWS = 62
PREFIX_EPOCHS = 2700
ESN0_DB = 10.0
NO_TEMPLATE = np.array([], dtype=np.float32)


def run_case(label, static_doppler_hz, rate_hz_per_s, seed):
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, _data_bits = make_ramp_signal(
        cn0_dbhz, seed, n_sym=8000, rate_hz_per_s=rate_hz_per_s,
        static_doppler_hz=static_doppler_hz,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen)

    acq = Acquisition(
        CODE, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ, pfa=1e-3, pd=0.9,
        symbol_rate=SYM_RATE,
    )
    handoff, consumed = search_and_handoff(acq, x, SPC, FS_FRONT)

    def true_doppler_at(n_front_samples):
        return static_doppler_hz + rate_hz_per_s * (n_front_samples / FS_FRONT)

    tracker_init_chip = (SF - handoff.chip_phase) % SF
    coarse_norm_freq = handoff.doppler_hz_est / FS_FRONT

    tail = x[consumed:]
    n_epochs_total = len(tail) // TE
    n_prefix_epochs = min(PREFIX_EPOCHS, n_epochs_total // 4)
    prefix = tail[: n_prefix_epochs * TE]
    true_residual_hz = (
        true_doppler_at(consumed + n_prefix_epochs * TE)
        - handoff.doppler_hz_est
    )

    out0, window_rate_hz = _collect_frozen_carrier_prefix(
        CoupledAsyncDespreader, CODE, SPC, BN_CODE, coarse_norm_freq,
        prefix, FS_FRONT, BN_CARRIER, WINDOWS, tracker_init_chip,
    )

    py_residual_hz = estimate_residual_freq_matched(
        out0, window_rate_hz, SYM_RATE, n_fft=64, zero_pad=4, interp=True,
    )

    # Match Python's own n_fft=64 choice explicitly: the auto default
    # (symbol_rate_hz/10) assumes window_rate_hz isn't wildly larger
    # than symbol_rate_hz, which doesn't hold at this real operating
    # point (window_rate_hz=186kHz vs symbol_rate_hz=2700Hz) -- an
    # apples-to-apples comparison needs the same coherent-block size,
    # not two different resolution choices. Every other param stays at
    # the object's own recommended defaults (sequential=True,
    # design_snr=2.0) -- see the module docstring for why tuning
    # design_snr further didn't produce a clean, trustworthy result.
    ca = CarrierAcquisition(
        NO_TEMPLATE, window_rate_hz, SYM_RATE,
        resolution_hz=window_rate_hz / 64.0, zero_pad=4,
    )
    ca.steps(out0.astype(np.complex64))

    print(f"--- {label} ---")
    print(f"  window_rate_hz={window_rate_hz:.0f}  len(out0)={len(out0)}  "
          f"ca.nfft={ca.nfft}  ca.dwell_target={ca.dwell_target}")
    print(f"  true residual (at prefix end): {true_residual_hz:+.1f} Hz")
    print(f"  Python (estimate_residual_freq_matched): "
          f"{py_residual_hz:+.1f} Hz  "
          f"(err {py_residual_hz - true_residual_hz:+.1f} Hz)")
    if ca.ready:
        print(f"  C (CarrierAcquisition, n_blocks={ca.n_blocks}): "
              f"{ca.residual_hz:+.1f} Hz  "
              f"(err {ca.residual_hz - true_residual_hz:+.1f} Hz)")
        print(f"  C vs Python agreement: "
              f"{abs(ca.residual_hz - py_residual_hz):.1f} Hz")
    else:
        print(f"  C (CarrierAcquisition): NOT ready after "
              f"{ca.n_blocks}/{ca.max_n_blocks} blocks")
    print("  (reporting only -- see module docstring: the CFAR-calibration "
          "question is a documented open item, not asserted pass/fail here)")


CASES = (
    ("zero impairments", 0.0, 0.0),
    ("static +15kHz offset", 15000.0, 0.0),
    ("SPEC real rate (500Hz/s)", 0.0, RATE_HZ_PER_S),
)


def main():
    """Run ONE case per process invocation (argv[1] = case index) --
    each case's own heavy signal-generation arrays are released when
    THIS process exits, instead of accumulating across a 3-case loop in
    one process (see feedback_wsl_memory_guard_large_arrays memory: a
    prior version of this script looping all 3 cases in one process
    OOM-crashed the whole WSL VM even though no single case's own
    parameters were scaled up from already-safe prior runs)."""
    import sys

    idx = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    label, static_hz, rate = CASES[idx]
    print(f"=== C CarrierAcquisition vs Python estimate_residual_freq_matched "
          f"(case {idx}/{len(CASES) - 1}) ===")
    run_case(label, static_hz, rate, seed=7000 + idx)


if __name__ == "__main__":
    main()
