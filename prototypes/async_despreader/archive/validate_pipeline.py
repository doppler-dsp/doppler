"""Full closed-loop pipeline validation: SimpleAsyncDespreader -> Costas
-> Resampler -> SymbolSync -> bit decisions, at the same link budget as
`src/doppler/track/tests/test_async_dsss_receiver.py` (2.046 Mcps,
1023-chip code, 1800 bps -> ~30.6 dB processing gain).

Requires doppler installed (`from doppler.track import ...`).

Key finding versus the test file's own `_recover()`: `NOMINAL_RATE =
windows * EPOCHS_PER_SYMBOL` is NOT an integer (e.g. 6 * 10/9 = 6.667),
so feeding the despreader's raw chunk-rate output straight into
`SymbolSync` (as the original test does) makes it track a fractional,
non-integer samples/symbol rate directly -- this recovers bits cleanly
most of the time but was seen to fail at one specific code-phase offset
(BER ~0.10). Explicitly resampling to a clean integer samples/symbol
BEFORE `SymbolSync` (matching the `feedback_despread_resample_demod_
separation` principle -- don't let one stage's tuning parameter double
as a downstream stage's rate requirement) fixed that: BER=0.0 across
every phase offset and symbol-clock-drift scenario tested.

SNR sweep note: both `Costas` and `SymbolSync` need several inverse-
loop-bandwidths of samples to settle before their output is trustworthy
(`1/bn` chunks/symbols respectively -- at `bn=0.02` that's ~50 units,
but cycle-slip-driven false starts can need several multiples of that,
especially at low SNR where the loop's per-sample input SNR, at the
*chunk* rate, is much lower than the eventual matched-filtered symbol
SNR). A short run with a naive "discard the first quarter" warm-up
undercounts this at low Es/N0 and reports a misleadingly high BER that
isn't a real implementation loss -- confirmed by a genie-timing,
no-carrier test of the despreader ALONE at Es/N0=10dB (0 errors in
~4000 symbols; theoretical BPSK BER there is ~3.9e-6) and, once the
warm-up is sized correctly (`SNR_SWEEP_WARMUP`, several multiples of
`1/bn`) and the run is long enough for enough post-warm-up symbols to
matter statistically, the full blind pipeline too (0 errors in 6191
symbols at Es/N0=10dB in a 20,000-symbol run).
"""
from __future__ import annotations

import numpy as np

from despreader import SimpleAsyncDespreader
from doppler.resample import Resampler
from doppler.track import Costas, SymbolSync

CHIP_RATE = 2.046e6
SF = 1023
SPS = 2
TE = SF * SPS
DATA_RATE = 1800.0
EPOCHS_PER_SYMBOL = (1.0 / DATA_RATE) / (SF / CHIP_RATE)
F0 = 1e-4
WINDOWS = 6  # a divisor of TE=2046 (the original test's K=8 is not)
TARGET_SPS = 8


def code(seed=11):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def signal(c, nsym, epochs_per_symbol, phi, f0, snr_db, seed):
    rng = np.random.default_rng(seed)
    csign = np.where(c & 1, -1.0, 1.0)
    tsym = TE * epochs_per_symbol
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx // SPS) % SF
    rx = (data[si] * csign[cph] * np.exp(2j * np.pi * f0 * idx)).astype(
        np.complex64
    )
    if snr_db is not None:
        p = np.sqrt(np.mean(np.abs(rx) ** 2))
        std = np.sqrt(10 ** (-snr_db / 10)) * p
        rx = rx + (
            rng.normal(0, std / np.sqrt(2), n)
            + 1j * rng.normal(0, std / np.sqrt(2), n)
        ).astype(np.complex64)
    return rx, data, tsym


def ber(dec, truth, warmup=0):
    """BER over `dec[warmup:]` vs `truth`, aligned by cross-correlation.

    `warmup` must cover several inverse-loop-bandwidths of both `Costas`
    and `SymbolSync` settling time (see module docstring) -- too short a
    discard at low SNR reports a misleadingly high BER that reflects
    loop-acquisition transients, not steady-state performance.
    """
    dec = np.asarray(dec, float)[warmup:]
    c = np.correlate(dec, truth.astype(float), "full")
    k = int(np.argmax(np.abs(c)))
    lag = k - (len(truth) - 1)
    inv = np.sign(c[k])
    err = cnt = 0
    for i in range(len(dec)):
        j = i - lag
        if 0 <= j < len(truth):
            err += dec[i] != inv * truth[j]
            cnt += 1
    return (err / cnt if cnt else 1.0), cnt


def recover(rx, c, conv_rate):
    n_epochs = len(rx) // TE
    rx = rx[: n_epochs * TE].astype(np.complex128)
    d = SimpleAsyncDespreader(c, SPS, bn=0.002, zeta=0.707, windows=WINDOWS)
    part = d.run(rx).astype(np.complex64)
    cos = Costas(bn=0.02, zeta=0.707, tsamps=1)
    wiped = cos.steps(part).astype(np.complex64)
    mf = np.convolve(wiped, np.ones(WINDOWS), mode="same").astype(
        np.complex64
    )
    rs = Resampler(rate=conv_rate)
    resampled = rs.execute(mf)
    ss = SymbolSync(sps=TARGET_SPS, bn=0.02, zeta=0.707, order="cubic")
    syms = ss.steps(resampled)
    return syms, d, cos, ss


WARMUP = 300  # >> 1/bn=50 chunks/symbols at bn=0.02, comfortable at Es/N0~22.6dB
SNR_SWEEP_NSYM = 20000
SNR_SWEEP_WARMUP = 2000  # several multiples of 1/bn -- needed at low SNR


def main():
    c = code(11)
    conv_rate = TARGET_SPS / (WINDOWS * EPOCHS_PER_SYMBOL)

    print("--- code-phase offset sweep (Es/N0 ~ 22.6 dB) ---")
    for phi_frac in (0.1, 0.37, 0.63):
        rx, data, _ = signal(
            c, 1200, EPOCHS_PER_SYMBOL, phi_frac * TE, F0, -8, 5
        )
        syms, d, cos, ss = recover(rx, c, conv_rate)
        dec = np.where(syms.real >= 0, 1, -1)
        b, cnt = ber(dec, data, warmup=WARMUP)
        print(
            f"  phi_frac={phi_frac}  BER={b:.4e} (n={cnt})  "
            f"code_rate={d.code_rate:.6f}  cos.locked={cos.locked}  "
            f"ss.rate={ss.rate:.4f} (target {TARGET_SPS})"
        )

    print("--- symbol-clock drift sweep (independent, unknown timebase) ---")
    for drift_pct in (-1.0, -0.3, 0.3, 1.0):
        true_ratio = EPOCHS_PER_SYMBOL * (1 + drift_pct / 100)
        rx, data, _ = signal(c, 1200, true_ratio, 0.37 * TE, F0, -8, 5)
        syms, d, cos, ss = recover(rx, c, conv_rate)
        dec = np.where(syms.real >= 0, 1, -1)
        expected_rate = TARGET_SPS * (true_ratio / EPOCHS_PER_SYMBOL)
        b, cnt = ber(dec, data, warmup=WARMUP)
        print(
            f"  drift_pct={drift_pct:+.1f}  BER={b:.4e} (n={cnt})  "
            f"ss.rate={ss.rate:.4f} (expected ~{expected_rate:.4f})"
        )

    print("--- SNR sweep (long runs + a warm-up sized for loop settling) ---")
    pg_db = 10 * np.log10(CHIP_RATE / DATA_RATE)  # chips/symbol, in dB
    for esn0_db, label in [(22.6, "nominal"), (10.0, "low, theoretical BER~3.9e-6")]:
        snr_db = esn0_db - pg_db
        rx, data, _ = signal(
            c, SNR_SWEEP_NSYM, EPOCHS_PER_SYMBOL, 0.37 * TE, F0, snr_db, 3
        )
        syms, d, cos, ss = recover(rx, c, conv_rate)
        dec = np.where(syms.real >= 0, 1, -1)
        b, cnt = ber(dec, data, warmup=SNR_SWEEP_WARMUP)
        print(
            f"  Es/N0={esn0_db}dB ({label})  BER={b:.4e} (n={cnt})  "
            f"cos.locked={cos.locked}"
        )


if __name__ == "__main__":
    main()
