"""Quick check: does DsssReceiver's own _carrier_update_from_partials
sign-alignment removal regress tracking on case 1's own signal, the same
way AsyncDsssReceiver's analogous _track_period removal did? DsssReceiver
has NO refine stage -- it seeds tracking directly from Acquisition's own
coarse estimate (near-perfect on this case, 0.0Hz reported err), unlike
AsyncDsssReceiver's track stage, which is seeded from the refine stage's
own still-imperfect (~356Hz off) estimate. Testing whether that seed-
quality difference, not the discriminator itself, explains why removing
_track_period's sign-align broke tracking.
"""
from __future__ import annotations

import numpy as np

from spec_full_characterization import (
    CHIP_RATE, CODE, DOPPLER_UNCERTAINTY_HZ, FS_GEN, SPC, SYM_RATE,
    es_n0_to_cn0_dbhz, make_ramp_signal,
)

from doppler.dsss import DsssReceiver
from doppler.resample import RateConverter

SF = 1023
TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
ESN0_DB = 10.0
STATIC_DOPPLER_HZ = 15000.0
SEED = 8001


def _fit_evm_db(vals, idxs, data_bits, max_lag=50):
    n = len(vals)
    if n < 20:
        return None
    lo, hi = n // 3, 2 * n // 3
    best_err = np.inf
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
    return 20.0 * np.log10(np.sqrt(best_err) + 1e-12) if np.isfinite(best_err) else None


def main():
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)
    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz, SEED, n_sym=8000, rate_hz_per_s=0.0,
        static_doppler_hz=STATIC_DOPPLER_HZ,
    )
    x = (
        RateConverter(rate=FS_FRONT / FS_GEN)
        .execute(x_gen)
        .astype(np.complex64)
    )

    rx = DsssReceiver(
        CODE, chip_rate=CHIP_RATE, symbol_rate=SYM_RATE, spc=SPC,
        cn0_dbhz=cn0_dbhz, doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        segments=4, sps=8,
    )
    sym_chunks = []
    n_syms_so_far = 0
    idxs_list = []
    for pos in range(0, len(x) - TE, TE):
        out = rx.steps(x[pos : pos + TE])
        if len(out):
            sym_chunks.append(out)
            idxs_list.append(np.arange(n_syms_so_far, n_syms_so_far + len(out)))
            n_syms_so_far += len(out)

    syms = np.concatenate(sym_chunks) if sym_chunks else np.zeros(0, dtype=np.complex64)
    idxs = np.arange(len(syms))
    evm_db = _fit_evm_db(syms.astype(np.complex128), idxs, data_bits.astype(np.float64))

    print(f"DsssReceiver on case1: tracking={bool(rx.tracking)}  "
          f"doppler_hz={rx.doppler_hz:.1f}  lock={rx.lock:.3f}  "
          f"n_syms={len(syms)}  overall_evm_db={evm_db}")


if __name__ == "__main__":
    main()
