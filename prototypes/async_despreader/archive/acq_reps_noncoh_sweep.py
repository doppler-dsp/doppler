"""Can this be solved PURELY in `Acquisition` -- no separate Python
bridge at all? Per direct user redirect: the low-Es/N0 fix that worked
for `freq_refine.py`'s hand-rolled squaring+FFT bridge was multi-look
non-coherent averaging, not a longer coherent FFT or matched filtering
the data. `Acquisition` already HAS both knobs natively (`doppler_bins`
= coherent depth = the "FFT length" axis, `n_noncoh` = the averaging
axis) plus real code-phase correlation the hand-rolled bridge never
had. This script asks directly: with the right (`doppler_bins`,
`n_noncoh`) combination -- more repeats/looks, not a longer coherent
window -- can `Acquisition` alone resolve Doppler well enough (landing
within the coupled tracker's own ~100-200 Hz pull-in from
`pullin_sweep.py`) under REAL asynchronous 1800 sps data modulation, at
the same low Es/N0 operating points (2, 5 dB) that broke the bridge?

Uses this prototype folder's own established geometry
(`validate_stress.py`'s `CHIP_RATE`/`SF`/`SPS`/`DATA_RATE` = 2.046e6 Hz
/ 1023 / 2 / 1800 Hz -- consistent with every other script in this
folder) and the REAL `doppler.dsss.Acquisition` C engine (not a
reimplementation -- this is exactly the point: stop building parallel
Python machinery, use the canonical object).

One dwell attempt per trial (`nc` frames of `doppler_bins` epochs
each, matching exactly one non-coherent accumulation cycle -- the
same "one collection window" framing `characterize_snr.py`/
`improve_low_snr.py` used for the bridge, so results are directly
comparable): does `Acquisition.push()` fire a hit at all, and if so,
is the folded `doppler_bin` estimate within tolerance of the injected
truth?

Run: `python acq_reps_noncoh_sweep.py` (needs numpy + doppler).
"""
from __future__ import annotations

import numpy as np

from doppler.dsss import Acquisition
from validate_stress import code, CHIP_RATE, SF, SPS as SPC, DATA_RATE

ES_N0_DB_LIST = [2.0, 5.0]
D_LIST = [8, 16, 32]
NC_LIST = [1, 6, 12, 24, 48]
N_TRIALS = 8
DOPPLER_ERR_TOL_HZ = 150.0  # matches pullin_sweep.py's own ~100-200 Hz
# boundary -- "good enough to hand to the coupled tracker."

_SAMPLE_TO_SYMBOL_RATE_DB = 10.0 * np.log10((CHIP_RATE * SPC) / DATA_RATE)


def es_n0_to_cn0_dbhz(es_n0_db):
    # Established project relationship (dsss_acq_async_data_demo.py):
    # Es/N0 = C/N0 - 10*log10(symbol_rate).
    return es_n0_db + 10.0 * np.log10(DATA_RATE)


def make_signal(code_arr, cn0_dbhz, doppler_hz, seed, n_epochs, spc):
    """Continuous, asynchronous-data DSSS chip-rate capture -- same
    formula/convention as `dsss_acq_async_data_demo.py`'s own
    `make_signal` (AWGN via the identical `amp_snr = sqrt(10**(cn0/10)
    / fs)` per-sample relationship `Acquisition` itself uses for
    sizing, so `cn0_dbhz` here is directly what the engine is
    configured with -- not reimplemented, just reused at this
    project's own CHIP_RATE/SF geometry instead of duplicating the
    example script's different chip rate)."""
    rng = np.random.default_rng(seed)
    csign = np.where(code_arr & 1, -1.0, 1.0)
    sf = len(code_arr)
    te = sf * spc
    fs = CHIP_RATE * spc
    tsym = fs / DATA_RATE
    n = n_epochs * te + 2 * te
    idx = np.arange(n)
    n_sym_needed = int(n / tsym) + 4
    data = (rng.integers(0, 2, n_sym_needed) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / tsym).astype(int), 0, len(data) - 1)
    cph = (idx // spc) % sf
    sig = data[si] * csign[cph] * np.exp(
        2j * np.pi * (doppler_hz / fs) * idx
    )
    amp_snr = np.sqrt(10.0 ** (cn0_dbhz / 10.0) / fs)
    sigma = 1.0 / amp_snr
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )
    return (sig + noise).astype(np.complex64)


def fold_doppler_hz(dop_bin, doppler_bins, doppler_res_hz):
    half = doppler_bins // 2
    folded = (dop_bin + half) % doppler_bins
    k_fold = folded - half
    return k_fold * doppler_res_hz


def run_config(c, cn0_dbhz, doppler_bins, n_noncoh, n_trials, seed0):
    a = Acquisition(
        c, reps=max(doppler_bins, 1), spc=SPC, chip_rate=CHIP_RATE,
        cn0_dbhz=max(cn0_dbhz, 30.0), max_noncoh=max(n_noncoh, 1),
    )
    a.configure_search_raw(doppler_bins, n_noncoh)
    te = a.code_bins * a.doppler_bins
    doppler_res_hz = a.doppler_res_hz
    span = a.doppler_span_hz

    n_hit = 0
    n_within_tol = 0
    errs = []
    for trial in range(n_trials):
        rng = np.random.default_rng(seed0 + trial)
        true_doppler_hz = rng.uniform(-0.9 * span, 0.9 * span)
        x = make_signal(
            c, cn0_dbhz, true_doppler_hz, seed0 + trial + 90000,
            n_epochs=doppler_bins * n_noncoh, spc=SPC,
        )
        a.reset()
        hit = None
        for f in range(n_noncoh):
            hits = a.push(x[f * te:(f + 1) * te])
            if hits:
                hit = hits[0]
                break
        if hit is None:
            continue
        n_hit += 1
        dop_bin = hit[0]
        est_hz = fold_doppler_hz(dop_bin, doppler_bins, doppler_res_hz)
        err = abs(est_hz - true_doppler_hz)
        errs.append(err)
        if err < DOPPLER_ERR_TOL_HZ:
            n_within_tol += 1

    return {
        "hit_rate": n_hit / n_trials,
        "success_rate": n_within_tol / n_trials,
        "rms_err": (
            float(np.sqrt(np.mean(np.square(errs)))) if errs else None
        ),
        "doppler_res_hz": doppler_res_hz,
    }


def main():
    c = code(11)
    print(
        f"=== Acquisition reps/n_noncoh sweep, async {DATA_RATE:.0f} sps "
        f"data, tolerance={DOPPLER_ERR_TOL_HZ:.0f} Hz ==="
    )
    for es_n0_db in ES_N0_DB_LIST:
        cn0_dbhz = es_n0_to_cn0_dbhz(es_n0_db)
        print(f"\n--- Es/N0={es_n0_db:.1f} dB (C/N0={cn0_dbhz:.1f} "
              f"dB-Hz) ---")
        for doppler_bins in D_LIST:
            for n_noncoh in NC_LIST:
                r = run_config(
                    c, cn0_dbhz, doppler_bins, n_noncoh, N_TRIALS,
                    seed0=1000 + doppler_bins * 100 + n_noncoh,
                )
                rms = (
                    f"{r['rms_err']:7.2f} Hz" if r["rms_err"] is not None
                    else "    n/a"
                )
                print(
                    f"  D={doppler_bins:3d} (res={r['doppler_res_hz']:6.1f} Hz)  "
                    f"nc={n_noncoh:3d}  total_epochs={doppler_bins*n_noncoh:5d}  "
                    f"hit_rate={r['hit_rate']:.2f}  "
                    f"success(<{DOPPLER_ERR_TOL_HZ:.0f}Hz)={r['success_rate']:.2f}  "
                    f"RMS_err|hit={rms}"
                )


if __name__ == "__main__":
    main()
