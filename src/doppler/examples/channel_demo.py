"""channel_demo.py — a full GPS-style tracking channel, end to end.

Builds a continuous DSSS-BPSK signal — a 127-chip PN code spreading BPSK data,
with a residual carrier offset, a slow code Doppler, and AWGN — and runs
:class:`doppler.track.Channel`, which composes a carrier loop
(:class:`~doppler.track.Costas`, FLL-assisted) and a code loop
(:class:`~doppler.track.Dll`) on one shared per-sample integrate-and-dump. The
carrier residual exceeds a bare PLL's pull-in, so the FLL assist is on.

Three views (saved to a PNG):
  * **Carrier** — the NCO frequency estimate pulling onto the true residual
    (dashed), with the lock metric ramping to 1.
  * **Code** — the chip-rate estimate tracking the true code Doppler (dashed).
  * **Soft decisions** — the despread prompt symbol's real part, dots coloured
    by whether they match the transmitted bit (a global 180deg flip is
    don't-care). The recovered bit-error rate is annotated.

Run:  python -m doppler.examples.channel_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import Channel

SF, SPS = 127, 8
TSAMPS = SF * SPS
NPER = 1500  # code periods (= data symbols, nav_period=1)
F0 = 0.18 / TSAMPS  # residual carrier offset (FLL territory)
CODE_DOPPLER = 5e-5  # code rate error
SNR_DB = 8.0


def _signal(code, seed=0):
    rng = np.random.default_rng(seed)
    data = rng.integers(0, 2, NPER) * 2 - 1
    n = TSAMPS * NPER
    rx = np.empty(n, np.complex64)
    cph = 0.0
    k = 0
    for p in range(NPER):
        for _ in range(TSAMPS):
            idx = int(cph % SF)
            rx[k] = data[p] * (-1.0 if code[idx] & 1 else 1.0)
            cph += (1.0 + CODE_DOPPLER) / SPS
            k += 1
    rx = rx * np.exp(2j * np.pi * F0 * np.arange(n))
    sigma = np.sqrt(10.0 ** (-SNR_DB / 10.0) / 2.0)
    rx = rx + (rng.normal(0, sigma, n) + 1j * rng.normal(0, sigma, n))
    return rx.astype(np.complex64), data


def main(out_path="channel_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    code = np.random.default_rng(1).integers(0, 2, SF).astype(np.uint8)
    rx, data = _signal(code, seed=9)
    ch = Channel(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.03, 0.707, 0.5, 1)

    freq = np.empty(NPER)
    lock = np.empty(NPER)
    rate = np.empty(NPER)
    sym = np.empty(NPER)
    for p in range(NPER):
        s = ch.steps(rx[p * TSAMPS : (p + 1) * TSAMPS])
        freq[p] = ch.norm_freq
        lock[p] = ch.lock_metric
        rate[p] = ch.code_rate
        sym[p] = s[0].real if len(s) else np.nan

    # ambiguity-tolerant BER on the converged tail
    tail = NPER // 2
    dec = np.where(sym[tail:] >= 0, 1, -1)
    err = int(np.sum(dec != data[tail:]))
    ber = min(err, len(dec) - err) / len(dec)
    flip = err > len(dec) - err  # recovered with a global inversion?

    t = np.arange(NPER)
    fig, (a, b, c) = plt.subplots(3, 1, figsize=(9, 8), sharex=True)

    a.axhline(F0 * 1e3, color="k", ls="--", lw=1.3, label="true residual")
    a.plot(t, freq * 1e3, color="#1f77b4", lw=1.1, label="NCO freq")
    a.plot(t, lock, color="#d62728", lw=1.0, label="lock metric")
    a.set_ylabel("millicycles/sample · lock")
    a.set_title(
        f"Tracking channel (Costas+FLL + DLL): carrier residual "
        f"{F0 * TSAMPS:.2f} cyc/epoch, SNR {SNR_DB:.0f} dB",
        fontsize=10,
    )
    a.legend(fontsize=8, loc="center right")
    a.grid(alpha=0.3)

    b.axhline(1 + CODE_DOPPLER, color="k", ls="--", lw=1.3, label="true rate")
    b.plot(t, rate, color="#2ca02c", lw=1.1, label="DLL code rate")
    b.set_ylabel("code rate")
    b.set_title("Code loop tracking the chip-rate Doppler", fontsize=10)
    b.legend(fontsize=8, loc="lower right")
    b.grid(alpha=0.3)

    # full-range soft decisions, coloured by correctness (resolve the global
    # 180deg ambiguity from the tail): pull-in errors then clean data.
    ref = data if not flip else -data
    all_dec = np.where(sym >= 0, 1, -1)
    good = all_dec == ref
    c.scatter(t[good], sym[good], s=4, color="#2ca02c", label="correct")
    if (~good).any():
        c.scatter(t[~good], sym[~good], s=10, color="#d62728", label="error")
    c.axhline(0, color="0.6", lw=0.6)
    c.set_ylabel("prompt Re")
    c.set_xlabel("code period (= data symbol)")
    c.set_title(
        f"Soft decisions vs time — pull-in then clean data, "
        f"amb-BER (tail) = {ber:.3g}",
        fontsize=10,
    )
    c.legend(fontsize=8, loc="center right")
    c.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}  (amb-BER={ber:.3g})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "channel_demo.png")
