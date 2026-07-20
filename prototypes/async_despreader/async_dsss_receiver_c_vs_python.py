"""A/B validation for the new C `doppler.dsss.AsyncDsssReceiver`
(Acquisition -> handoff -> `CarrierAcquisition` refine -> per-code-period
Costas/Dll/RateConverter/MpskReceiver track, one object --
~/.claude/plans/crystalline-knitting-hopper.md) against the already-
validated Python prototype pipeline it ports,
`e2e_acq_to_despreader.py`'s own `run_trial()` (Acquisition search+handoff
-> `refine_seed_carrier_acq` -> `CoupledAsyncDespreader` tracking).

Both are fed the IDENTICAL front-end signal, at `SPEC.md`'s own real
operating point (Gold-1023 @ 3.069 Mcps / async BPSK @ 2700 bps, the
genuine +/-50kHz Acquisition search, the real 500 Hz/s Doppler rate),
across the same three gating cases `e2e_acq_to_despreader.py` itself
uses. Reports BER for both side by side rather than asserting bit-exact
agreement (the two pipelines demodulate through different downstream
stages -- the Python side stops at the despread OUTPUT and is scored via
a genie-aided bit sync, the C side runs a real `MpskReceiver` all the way
to symbol decisions -- so consistent PASS/FAIL behavior at each case is
the actual claim, not sample-for-sample identity).

Run: `python async_dsss_receiver_c_vs_python.py <case_index>` (needs
numpy + doppler; ONE case per process -- see
feedback_wsl_memory_guard_large_arrays memory on why this script never
loops multiple cases in one process).

**Result (all 3 gating cases run)**: case 0 (zero impairments) and case 2
(SPEC's own real 500Hz/s combined scenario -- the decisive case for task
#99) AGREE, both PASS (BER~0.0003-0.0004). Case 1 (static +15kHz offset)
DISAGREES: Python passes (BER=0.0000) but the C side fails (BER~0.48,
chance level). Directly confirmed this is NOT a regression from
AsyncDsssReceiver's own new machinery -- the ALREADY-SHIPPED
`DsssReceiver`, given the identical signal, fails IDENTICALLY (BER~0.48,
`doppler_hz` reads back exactly 15000.0 -- the coarse estimate itself is
correct). A pre-existing bug shared by both C receivers' common downstream
chain when the coarse Doppler is large (not near zero) -- neither
receiver's own test suite exercises SPEC's real +/-50kHz search width at
a large offset, so this was never caught before. Root cause not yet
found; tracked as a new, separate follow-up (not task #99, which cases 0
and 2 already close).
"""

from __future__ import annotations

import numpy as np

from e2e_acq_to_despreader import run_trial
from spec_full_characterization import (
    CHIP_RATE,
    CODE,
    DOPPLER_UNCERTAINTY_HZ,
    FS_GEN,
    RATE_HZ_PER_S,
    SF,
    SPC,
    SYM_RATE,
    es_n0_to_cn0_dbhz,
    make_ramp_signal,
)

from doppler.dsss import AsyncDsssReceiver
from doppler.resample import RateConverter

TE = SF * SPC
FS_FRONT = CHIP_RATE * SPC
ESN0_DB = 10.0
N_SYM = 8000


def _best_ber(syms, data_bits):
    if len(syms) < 20:
        return 1.0, 0
    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 2, len(bits)
    best = 1.0
    best_n = 0
    for lag in range(-100, 101):
        ti = lag + np.arange(lo, hi)
        mask = (ti >= 0) & (ti < len(data_bits))
        if mask.sum() < (hi - lo) // 2:
            continue
        truth = data_bits[ti[mask]] * 2.0 - 1.0
        ber_fwd = float(np.mean(bits[lo:hi][mask] != truth))
        ber_inv = float(np.mean(bits[lo:hi][mask] != -truth))
        if min(ber_fwd, ber_inv) < best:
            best = min(ber_fwd, ber_inv)
            best_n = int(mask.sum())
    return best, best_n


def run_c_case(x, cn0_dbhz, data_bits, doppler_uncertainty):
    rx = AsyncDsssReceiver(
        CODE,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=doppler_uncertainty,
        segments=4,
        sps=8,
    )
    syms = []
    for pos in range(0, len(x) - TE, TE):
        out = rx.steps(x[pos : pos + TE])
        if len(out):
            syms.append(out)
    syms = np.concatenate(syms) if syms else np.zeros(0, dtype=np.complex64)
    ber, n_scored = _best_ber(syms, data_bits)
    return {"ber": ber, "n_scored": n_scored, "tracking": bool(rx.tracking)}


def run_case(label, static_doppler_hz, rate_hz_per_s, aid_code, seed):
    cn0_dbhz = es_n0_to_cn0_dbhz(ESN0_DB, sym_rate=SYM_RATE)

    py_result = run_trial(
        ESN0_DB,
        seed,
        static_doppler_hz=static_doppler_hz,
        rate_hz_per_s=rate_hz_per_s,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        aid_code=aid_code,
    )

    x_gen, data_bits = make_ramp_signal(
        cn0_dbhz,
        seed,
        n_sym=N_SYM,
        rate_hz_per_s=rate_hz_per_s,
        static_doppler_hz=static_doppler_hz,
    )
    x = RateConverter(rate=FS_FRONT / FS_GEN).execute(x_gen).astype(
        np.complex64
    )
    c_result = run_c_case(x, cn0_dbhz, data_bits, DOPPLER_UNCERTAINTY_HZ)

    print(f"--- {label} ---")
    print(
        f"  Python (e2e_acq_to_despreader.run_trial): "
        f"tracking={py_result['tracking']}  ber={py_result['ber']:.4f}  "
        f"n_scored={py_result.get('n_scored', 0)}"
    )
    print(
        f"  C (AsyncDsssReceiver):                    "
        f"tracking={c_result['tracking']}  ber={c_result['ber']:.4f}  "
        f"n_scored={c_result['n_scored']}"
    )
    both_pass = py_result["ber"] < 0.05 and c_result["ber"] < 0.05
    both_fail = py_result["ber"] >= 0.05 and c_result["ber"] >= 0.05
    agree = "AGREE" if (both_pass or both_fail) else "DISAGREE"
    print(f"  pass/fail agreement: {agree}")


CASES = (
    ("zero impairments", 0.0, 0.0, True),
    ("static offset only (aid_code=False)", 15000.0, 0.0, False),
    ("SPEC real rate (500Hz/s)", 0.0, RATE_HZ_PER_S, False),
)


def main():
    """Run ONE case per process invocation (argv[1] = case index) -- see
    the module docstring / feedback_wsl_memory_guard_large_arrays memory
    on why this never loops all cases in one process."""
    import sys

    idx = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    label, static_hz, rate, aid_code = CASES[idx]
    print(
        f"=== C AsyncDsssReceiver vs Python e2e_acq_to_despreader "
        f"(case {idx}/{len(CASES) - 1}) ==="
    )
    run_case(label, static_hz, rate, aid_code, seed=8000 + idx)


if __name__ == "__main__":
    main()
