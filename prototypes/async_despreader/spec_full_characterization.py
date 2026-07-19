"""Characterizes the REAL C `DsssReceiver` (post this session's `bn_fll`
pre-despread carrier fix, task #93/CHECKPOINT 11) against `SPEC.md`'s FULL
COMBINED operating point in one run -- the first time every SPEC dimension
has been exercised together against the actual production object:

  - real CCSDS Gold-1023 code (`doppler.wfm.Gold`, not a PRBS stand-in --
    `doppler.examples.dsss_receiver_stress.py`'s own established generator)
  - `chip_rate=3.069e6`, `symbol_rate=2700` (SPEC's own async BPSK numbers
    -- `chip_rate/(SF*symbol_rate)` ~= 1.111 periods/symbol, genuinely
    non-integer, i.e. truly asynchronous)
  - `doppler_uncertainty=50000.0` (SPEC's own +/-50kHz search range --
    forces `Acquisition`'s real wideband D=1 roll-FFT search; every
    earlier test in this story either used a much narrower uncertainty
    or bypassed Acquisition/seeded from ground truth)
  - `rate_hz_per_s=500.0` (SPEC's corrected Doppler-RATE worst case)
  - Es/N0 swept over {3, 5, 10, 20} dB (SPEC's own floor plus context
    points), converted to `DsssReceiver`'s own `cn0_dbhz` sizing input via
    `cn0_dbhz = esn0_db + 10*log10(symbol_rate)` (Es/N0 = C/N0 / R_sym).

Every earlier validation in this story tested a SUBSET: the Python floor
sweep (`doppler_rate_floor_test.py`) swept real Es/N0 but never touched
the actual C `DsssReceiver` or a wideband Acquisition search; the isolated
C composition test and the new `test_sustained_doppler_rate` C test (task
#97) exercised the real object at SPEC's rate/async-data numbers but at a
comfortable, narrow-uncertainty operating point, not the literal Es/N0
floor or the real +/-50kHz search. This script closes that gap.

Reuses `doppler.examples.dsss_receiver_stress`'s own established wfmgen
composition pattern (real Gold code, `Synth`-based chip*data composition
+ AWGN via `snr_mode="fs"`) -- not reinvented. One addition: wfmgen's own
`type="symbols"` Doppler mixing only supports a STATIC `freq`, no rate, so
the Doppler RAMP is applied as a direct post-multiply chirp on the
zero-Doppler, AWGN-injected wfmgen signal (the same ramp-phase convention
this session's own `test_real_costas_fll.py`/`doppler_rate_test.py`
already validated: `phase(t) = 2*pi*0.5*rate*t^2`).

Run: `python spec_full_characterization.py` (needs numpy + doppler).
"""

from __future__ import annotations

import math

import numpy as np

from doppler.dsss import DsssReceiver
from doppler.resample import RateConverter
from doppler.wfm import Gold, Synth

# ── SPEC.md's own numbers, exactly ──────────────────────────────────────
SF = 1023
CHIP_RATE = 3.069e6  # SPEC.md's own Gold-1023 rate
SYM_RATE = 2700.0  # SPEC.md's own async BPSK rate
DOPPLER_UNCERTAINTY_HZ = 50000.0  # SPEC's own +/-50kHz
RATE_HZ_PER_S = 500.0  # SPEC's corrected Doppler-rate worst case
ESN0_LIST_DB = (3.0, 5.0, 10.0, 20.0)

SPC = 2
N_SYM = 2000  # long enough for a meaningful ramp + settle margin
N_SEEDS = 4  # per Es/N0 point

# Generation rate: the exact LCM of chip_rate and symbol_rate (integer
# oversample for both, non-integer ratio between them -- the async walk),
# same technique dsss_receiver_stress.py's own make_signal_wfmgen uses.
FS_GEN = float(math.lcm(int(CHIP_RATE), int(SYM_RATE)))
CHIP_SPS = int(FS_GEN / CHIP_RATE)
DATA_SPS = int(FS_GEN / SYM_RATE)
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)

PERIODS_PER_SYMBOL = CHIP_RATE / (SF * SYM_RATE)


def es_n0_to_cn0_dbhz(esn0_db, sym_rate=SYM_RATE):
    """C/N0 (dB-Hz) = Es/N0 (dB) + 10*log10(symbol_rate) -- Es/N0 =
    (Es*R_sym)/N0 = C/N0 / R_sym, the same conversion this story's earlier
    Python floor sweep used (its own Es/N0=3dB point landed at
    cn0_dbhz=37.31, matching this formula at that sweep's symbol rate)."""
    return esn0_db + 10.0 * np.log10(sym_rate)


def make_ramp_signal(
    cn0_dbhz,
    seed,
    n_sym=N_SYM,
    rate_hz_per_s=RATE_HZ_PER_S,
    static_doppler_hz=0.0,
):
    """SPEC's own Gold-1023 code x random data, wfmgen-composed AWGN at
    zero static Doppler (matching dsss_receiver_stress.py's own pattern),
    plus a direct post-multiply Doppler RAMP -- wfmgen's own
    `type="symbols"` mixing has no rate parameter, so the ramp is added
    here, not through wfmgen's own Doppler mixing. `rate_hz_per_s`
    defaults to SPEC's own RATE_HZ_PER_S (every existing caller's
    behavior is unchanged); pass 0.0 for a zero-Doppler-rate control
    signal (still zero STATIC Doppler too, per the `freq=0.0` above --
    a true no-Doppler-impairment baseline). `static_doppler_hz` adds a
    CONSTANT offset on top of the ramp (default 0.0, every existing
    caller unchanged) -- needed to exercise Acquisition's own real
    +/-50kHz wideband search with a genuinely nonzero true residual to
    find, rather than the `freq=0.0`-only signal every earlier
    zero/rate-only ablation in this story used."""
    rng = np.random.default_rng(seed)
    data_bits = rng.integers(0, 2, n_sym).astype(np.uint8)
    n_total = n_sym * DATA_SPS

    chip_stream = Synth(
        type="bits",
        bits=CODE.tobytes(),
        modulation="bpsk",
        sps=CHIP_SPS,
        fs=FS_GEN,
    ).steps(n_total)
    data_stream = Synth(
        type="bits",
        bits=data_bits.tobytes(),
        modulation="bpsk",
        sps=DATA_SPS,
        fs=FS_GEN,
    ).steps(n_total)
    composite = (chip_stream * data_stream).astype(np.complex64)

    # `Synth(..., snr_mode="fs")` interprets `snr` as a PLAIN, total-noise-
    # power-over-the-whole-fs-Nyquist-band ratio (native/src/wfm_synth/
    # wfm_synth_core.c:89-102 -- mode==1: `snr_fs = snr`, a direct
    # passthrough, no fs-dependent scaling at all) -- NOT C/N0 in dB-Hz.
    # C/N0 (dB-Hz) = snr_fs (dB) + 10*log10(fs) (total noise power =
    # N0*fs), so passing `cn0_dbhz` directly here (as an earlier draft of
    # this function did, and as the module docstring above still
    # describes for narrative purposes) understates the injected noise by
    # `10*log10(FS_GEN)` (~70 dB at this script's own FS_GEN) -- verified
    # empirically (measured signal/noise power directly): the
    # uncorrected call actually injected noise equivalent to ~107 dB-Hz
    # when the label said 37.31 dB-Hz. Convert before passing.
    snr_fs_db = cn0_dbhz - 10.0 * np.log10(FS_GEN)
    out = Synth(
        type="symbols",
        symbols=composite,
        sps=1,
        freq=0.0,
        snr=snr_fs_db,
        snr_mode="fs",
        seed=seed,
        fs=FS_GEN,
    ).steps(n_total)

    t = np.arange(n_total) / FS_GEN
    ramp_phase = 2.0 * np.pi * (static_doppler_hz * t + 0.5 * rate_hz_per_s * t * t)
    out = (out * np.exp(1j * ramp_phase)).astype(np.complex64)
    return out, data_bits


def _lag_search_ber(bits, data_bits, max_lag=50):
    """Best-of-both-polarities BER over a lag search -- same convention
    dsss_receiver_stress.py's own _lag_search_ber already uses."""
    truth = np.where(data_bits > 0, -1.0, 1.0)
    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    best = 1.0
    for lag in range(-max_lag, max_lag + 1):
        ti = lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(truth):
            continue
        e_same = float(np.mean(bits[lo:hi] != truth[ti]))
        e_inv = float(np.mean(bits[lo:hi] != -truth[ti]))
        best = min(best, e_same, e_inv)
    return best


def _theory_ber_bpsk(esn0_db):
    """Coherent-BPSK AWGN theory bound: Pb = Q(sqrt(2*Es/N0)) -- the
    despreading gain and spreading factor both cancel out of this ratio,
    so this is the same reference curve regardless of SF/chip_rate; any
    gap above it is tracking loss (residual carrier/code error), not a
    fundamental limit."""
    lin = 10.0 ** (esn0_db / 10.0)
    return 0.5 * math.erfc(math.sqrt(lin))


def _lag_search_metrics(syms, data_bits, max_lag=50):
    """Best-lag/best-polarity BER (as `_lag_search_ber`) PLUS the EVM (dB)
    at that SAME winning (lag, polarity) -- computed against the ideal
    +/-1 BPSK reference after a least-squares complex gain fit (absorbs
    the receiver's own arbitrary output scale and any static residual
    carrier phase Costas didn't zero), the standard EVM normalization:
    EVM_rms = RMS(measured/gain - reference) / RMS(reference), reference
    RMS = 1 for a +/-1 BPSK constellation. This is a proper superset of
    BER -- unlike BER (which saturates at ~0.5 and can't distinguish
    "barely wrong" from "wildly wrong"), EVM keeps discriminating
    tracking quality even once bit decisions are already unreliable."""
    truth = np.where(data_bits > 0, -1.0, 1.0)
    lo, hi = len(syms) // 3, 2 * len(syms) // 3
    bits = np.where(syms.real > 0, 1.0, -1.0)

    best_ber = 1.0
    best_lag = 0
    best_sign = 1.0
    for lag in range(-max_lag, max_lag + 1):
        ti = lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(truth):
            continue
        e_same = float(np.mean(bits[lo:hi] != truth[ti]))
        e_inv = float(np.mean(bits[lo:hi] != -truth[ti]))
        if e_same < best_ber:
            best_ber, best_lag, best_sign = e_same, lag, 1.0
        if e_inv < best_ber:
            best_ber, best_lag, best_sign = e_inv, lag, -1.0

    ti = best_lag + np.arange(lo, hi)
    ref = best_sign * truth[ti]
    z = syms[lo:hi].astype(np.complex128)
    gain = np.mean(np.conj(ref) * z)  # LS complex gain, |ref|=1 constant
    if np.abs(gain) < 1e-12:
        return best_ber, 0.0  # degenerate (no signal at all)
    err = z / gain - ref
    evm_rms = float(np.sqrt(np.mean(np.abs(err) ** 2)))
    evm_db = 20.0 * np.log10(evm_rms) if evm_rms > 0 else -np.inf
    return best_ber, evm_db


def run_trial(esn0_db, seed):
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)
    x_gen, data_bits = make_ramp_signal(cn0_dbhz, seed)

    fs_front = CHIP_RATE * SPC
    x = RateConverter(rate=fs_front / FS_GEN).execute(x_gen)

    rx = DsssReceiver(
        CODE,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        segments=4,
        sps=8,
    )

    te = SF * SPC
    parts = []
    for pos in range(0, len(x) - te, te):
        out = rx.steps(x[pos : pos + te])
        if len(out):
            parts.append(out)
    syms = (
        np.concatenate(parts) if parts else np.zeros(0, dtype=np.complex64)
    )

    record = {
        "esn0_db": esn0_db,
        "seed": seed,
        "tracking": rx.tracking,
        "n_syms": len(syms),
    }
    if len(syms) < 10:
        record["ber"] = 1.0
        record["evm_db"] = 0.0
        return record

    record["ber"], record["evm_db"] = _lag_search_metrics(syms, data_bits)
    return record


def main():
    print(
        "=== SPEC full combined characterization ===\n"
        f"chip_rate={CHIP_RATE:.4e} Hz  symbol_rate={SYM_RATE} Hz  "
        f"periods/symbol={PERIODS_PER_SYMBOL:.4f} (async)\n"
        f"doppler_uncertainty=+/-{DOPPLER_UNCERTAINTY_HZ:.0f} Hz  "
        f"rate={RATE_HZ_PER_S:.0f} Hz/s  N_SYM={N_SYM}  seeds/point={N_SEEDS}"
    )
    for esn0_db in ESN0_LIST_DB:
        cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)
        recs = [run_trial(esn0_db, 5000 + s) for s in range(N_SEEDS)]
        n_tracking = sum(r["tracking"] for r in recs)
        bers = [r["ber"] for r in recs]
        evms = [r["evm_db"] for r in recs]
        theory = _theory_ber_bpsk(esn0_db)
        # Theoretical EVM under IDEAL tracking is just the Es/N0 itself,
        # inverted: EVM_rms^2 = N0/Es -> EVM_dB = -esn0_db. Any measured
        # EVM above that reflects real tracking loss (residual carrier
        # phase error, code/timing error, intermittent unlock), not AWGN.
        theory_evm_db = -esn0_db
        print(
            f"\nEs/N0={esn0_db:5.1f} dB  (cn0_dbhz={cn0_dbhz:.2f})  "
            f"tracking={n_tracking}/{N_SEEDS}\n"
            f"  ber=[{', '.join(f'{b:.4f}' for b in bers)}]  "
            f"theory_ber={theory:.3e}\n"
            f"  evm_db=[{', '.join(f'{e:.2f}' for e in evms)}]  "
            f"theory_evm_db={theory_evm_db:.2f}"
        )


if __name__ == "__main__":
    main()
