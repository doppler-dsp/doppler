"""Follow-up to the "4x the FFT size" question: NOT by extending the
real coherent depth D (that lengthens the coherent window in time and
makes the data-modulation aliasing WORSE, per `acq_reps_noncoh_sweep
.py`'s D-vs-4D result) -- instead, zero-pad the SAME D real per-epoch
correlation values before the slow-time FFT ("stack zeros on
horizontally" next to the real samples, not gather more real samples).
This is the exact `zero_pad` technique `freq_refine.py` already used on
its own squared spectrum; the question here is whether it does
anything useful on `Acquisition`'s OWN (non-squared) slow-time axis.

Genie-code-phase isolation: `Acquisition`'s C object doesn't expose its
internal per-epoch correlation vector (only the final hit tuple), so
this reconstructs JUST that one piece directly in Python -- correlate
each epoch's raw samples against the code replica at the TRUE (known)
code phase, giving one complex "coherent epoch sum" per epoch, exactly
what feeds the engine's own slow-time FFT. No code-phase search, no
CFAR, no non-coherent combining -- deliberately minimal, isolating only
the zero-pad-vs-real-D question, not a reimplementation of Acquisition.

Prediction from first principles (to be checked against real numbers,
not assumed): the data modulation's own broadband spectrum is already
baked into these D real correlation values BEFORE any transform --
zero-padding only interpolates the SAME (already-aliased) spectrum more
finely. It should sharpen an ALREADY-correct bin pick (same as it did
for `freq_refine.py`'s squared spectrum) but not rescue a wrong one,
since it adds no new real information.

Run: `python acq_slowtime_zeropad_test.py` (needs numpy + doppler).
"""
from __future__ import annotations

import numpy as np

from acq_reps_noncoh_sweep import make_signal, es_n0_to_cn0_dbhz
from validate_stress import code, CHIP_RATE, SF, SPS as SPC

D_LIST = [8, 16]
ZERO_PAD_LIST = [1, 4]
ES_N0_DB_LIST = [2.0, 5.0]
N_TRIALS = 16
TOL_HZ = 150.0


def per_epoch_correlations(x, code_arr, spc, n_epochs):
    """One complex coherent-correlation value per epoch, against the
    code replica at code phase 0 -- the exact quantity `Acquisition`'s
    own slow-time FFT operates on per code-phase column, reconstructed
    here only at the (genie-known) true code phase."""
    csign = np.where(code_arr & 1, -1.0, 1.0).astype(np.complex128)
    replica = np.repeat(csign, spc)  # one replica chip value per sample
    te = len(replica)
    out = np.empty(n_epochs, dtype=np.complex128)
    for k in range(n_epochs):
        seg = x[k * te:(k + 1) * te]
        out[k] = np.sum(seg * np.conj(replica))
    return out


def estimate_doppler_hz(per_epoch, doppler_res_hz, zero_pad):
    n = len(per_epoch)
    n_padded = n * zero_pad
    padded = np.zeros(n_padded, dtype=np.complex128)
    padded[:n] = per_epoch
    spec = np.fft.fft(padded)
    peak_bin = int(np.argmax(np.abs(spec)))
    half = n_padded // 2
    folded = (peak_bin + half) % n_padded
    k_fold = folded - half
    # doppler_res_hz is the NATIVE (un-padded) bin width; the padded
    # FFT samples that same continuous spectrum `zero_pad`x more
    # finely, so scale down by zero_pad to get Hz/bin on the padded
    # array.
    return k_fold * (doppler_res_hz / zero_pad)


def run_one(c, cn0_dbhz, doppler_hz, seed, n_epochs, doppler_res_hz):
    x = make_signal(c, cn0_dbhz, doppler_hz, seed, n_epochs, SPC)
    per_epoch = per_epoch_correlations(x, c, SPC, n_epochs)
    results = {}
    for zero_pad in ZERO_PAD_LIST:
        est = estimate_doppler_hz(per_epoch, doppler_res_hz, zero_pad)
        results[zero_pad] = abs(est - doppler_hz)
    return results


def main():
    c = code(11)
    print("=== Zero-pad (not extend-D) Acquisition's slow-time FFT ===")
    print(f"(genie code phase; tolerance={TOL_HZ:.0f} Hz)")
    for es_n0_db in ES_N0_DB_LIST:
        cn0_dbhz = es_n0_to_cn0_dbhz(es_n0_db)
        print(f"\n--- Es/N0={es_n0_db:.1f} dB (C/N0={cn0_dbhz:.1f} "
              f"dB-Hz) ---")
        for D in D_LIST:
            doppler_res_hz = CHIP_RATE / (SF * D)
            span = doppler_res_hz * D / 2.0
            errs = {zp: [] for zp in ZERO_PAD_LIST}
            for trial in range(N_TRIALS):
                rng = np.random.default_rng(7000 + D * 31 + trial)
                true_hz = rng.uniform(-0.9 * span, 0.9 * span)
                r = run_one(
                    c, cn0_dbhz, true_hz, 7000 + D * 31 + trial + 500,
                    D, doppler_res_hz,
                )
                for zp in ZERO_PAD_LIST:
                    errs[zp].append(r[zp])
            line = f"  D={D:3d} (res={doppler_res_hz:6.1f} Hz)  "
            for zp in ZERO_PAD_LIST:
                arr = np.array(errs[zp])
                success = float(np.mean(arr < TOL_HZ))
                rms = float(np.sqrt(np.mean(np.square(arr))))
                line += (
                    f" zero_pad={zp}: success={success:.2f} "
                    f"RMS={rms:7.2f}Hz |"
                )
            print(line)


if __name__ == "__main__":
    main()
