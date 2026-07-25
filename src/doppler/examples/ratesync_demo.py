"""ratesync_demo.py — arbitrary-rate reception with a fused RRC matched filter.

Builds RRC-shaped BPSK at a deliberately awkward **non-integer** samples per
symbol (17.33389 — an ADC clock with no rational relationship to the symbol
clock), adds a sample-clock rate offset and noise, and hands it to
:class:`doppler.track.RateSync`.

`RateSync` builds the root-raised-cosine matched filter **as** the polyphase
bank of a resampler, so the arm its accumulator selects *is* the fractional
timing delay — one dot product does matched filtering and retiming together,
with no Farrow interpolator. Because that accumulator is a `double`, `sps` is
a `double`: no integer relationship between the two clocks is required.

Three views (saved to a PNG):
  * **Recovered constellation** — matched-filtered symbols at a fractional
    sps, tight on +/-1 once the loop locks.
  * **Tracked clock** — the recovered samples/symbol converging onto the true
    (non-integer, offset) rate.
  * **Timing S-curve** — the Gardner error against timing offset, measured
    from this very object: one stable lock per symbol, no half-symbol
    ambiguity.

Run:  python -m doppler.examples.ratesync_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.track import RateSync

SPS = 17.33389  # non-integer samples per symbol -- the whole point
BETA = 0.35
SPAN = 8
NSYM = 1200
CLOCK_PPM = 200.0  # the true rate is this far off the nominal
SNR_DB = 14.0


def rrc_h(t, beta=BETA):
    """Analytic RRC pulse at arbitrary (non-grid) times, in symbol periods."""
    t = np.asarray(t, float)
    out = np.empty_like(t)
    pt = np.pi * t
    at_zero = np.abs(t) < 1e-9
    at_sing = (
        np.abs(np.abs(t) - 1.0 / (4 * beta)) < 1e-9
        if beta > 0
        else np.zeros(t.shape, bool)
    )
    gen = ~(at_zero | at_sing)
    out[at_zero] = 1.0 - beta + 4.0 * beta / np.pi
    if at_sing.any():
        a = np.pi / (4 * beta)
        out[at_sing] = (beta / np.sqrt(2)) * (
            (1 + 2 / np.pi) * np.sin(a) + (1 - 2 / np.pi) * np.cos(a)
        )
    tg, ptg = t[gen], pt[gen]
    num = np.sin(ptg * (1 - beta)) + 4 * beta * tg * np.cos(ptg * (1 + beta))
    out[gen] = num / (ptg * (1 - (4 * beta * tg) ** 2))
    return out


def make_signal(sps, tau=0.37, seed=7, snr_db=SNR_DB):
    """RRC-shaped BPSK at a fractional sps, offset by tau, in AWGN."""
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, NSYM)
    a = (2 * bits - 1).astype(float)
    n = int(NSYM * sps)
    t = (np.arange(n) - tau) / sps
    x = np.zeros(n)
    for k, ak in enumerate(a):
        lo = max(0, int(np.ceil((k - SPAN) * sps + tau)))
        hi = min(n, int(np.floor((k + SPAN) * sps + tau)) + 1)
        if hi > lo:
            x[lo:hi] += ak * rrc_h(t[lo:hi] - k)
    if snr_db is None:  # noiseless — for characterising the detector itself
        return bits, x.astype(np.complex64)
    sigma = np.sqrt(np.mean(x**2) / (2 * 10 ** (snr_db / 10)))
    noise = sigma * (rng.standard_normal(n) + 1j * rng.standard_normal(n))
    return bits, (x + noise).astype(np.complex64)


# The receiver is built at the NOMINAL sps; the stream's true rate is
# CLOCK_PPM off it, and the loop has to find and hold the difference.
true_sps = SPS * (1.0 + CLOCK_PPM * 1e-6)
bits, rx_in = make_signal(true_sps)

rx = RateSync(sps=SPS, beta=BETA, span=SPAN, num_phases=1024, bn=0.005)

# Fed in chunks purely so the tracked rate can be sampled as it converges —
# state carries across calls, so this is identical to one steps(rx_in).
chunks = np.array_split(rx_in, 120)
parts, rate_trace, sym_count = [], [], []
for chunk in chunks:
    parts.append(rx.steps(chunk))
    rate_trace.append(rx.rate)
    sym_count.append(sum(len(p) for p in parts))
symbols = np.concatenate(parts)
# --8<-- [end:signal]


def evm_self(y):
    """Self-referenced EVM: each symbol against its OWN hard decision, so no
    reference sequence and no lag search is involved (a BER alone can report
    chance on a perfect demod if the lag search misses)."""
    d = np.sign(y.real)
    g = np.vdot(d, y) / np.vdot(d, d)
    return float(
        np.sqrt(np.mean(np.abs(y - g * d) ** 2) / np.mean(np.abs(g * d) ** 2))
    )


def ber_wide(bits, y):
    """BER with a lag search wide enough to actually find the alignment."""
    tx = (2 * bits.astype(int) - 1).astype(float)
    d = np.sign(y.real).astype(float)
    n = 1 << int(np.ceil(np.log2(len(tx) + len(d))))
    c = np.fft.irfft(np.fft.rfft(tx, n) * np.conj(np.fft.rfft(d, n)), n)
    lag = int(np.argmax(np.abs(c)))
    s = 1.0 if c[lag] >= 0 else -1.0
    m = min(len(d), len(tx) - lag)
    return float(np.mean(s * d[:m] != tx[lag : lag + m]))


def s_curve(points=21):
    """Measure the timing S-curve from the object itself: freeze the loop at
    a series of static timing offsets and average EVERY symbol's TED error,
    read back through the object's own telemetry.

    Three things this measurement has to get right, each of which produced a
    convincing-looking but wrong curve first:

    * Sweep a **fractional** delay (`tau`, in samples). Slicing the sample
      array instead quantises the sweep to whole samples, and at a
      non-integer sps consecutive points then collapse onto the same
      timing — plateaus that belong to the measurement, not the detector.
    * Average **every** symbol's error, not `timing_error` sampled at a few
      chunk boundaries: the per-symbol error has a large data-pattern
      variance (Gardner only learns from transitions), so a few dozen
      samples leave enough noise to fake an asymmetric curve.
    * Measure it **noiseless**. Channel noise adds variance without changing
      the shape, so it only obscures what is being characterised.

    Done that way the curve is the standard Gardner S-curve: one period per
    symbol, odd-symmetric, zeros exactly half a symbol apart.
    """
    from doppler.telemetry import Telemetry

    offs = np.linspace(0.0, 1.0, points)
    err = []
    for o in offs:
        # tau is in samples; one symbol of timing == SPS samples
        _, x = make_signal(SPS, tau=o * SPS, seed=11, snr_db=None)
        # bn = 0 freezes the loop: the strobe stays where the offset put it
        probe = RateSync(
            sps=SPS, beta=BETA, span=SPAN, num_phases=1024, bn=0.0
        )
        tlm = Telemetry(1 << 16)
        probe.set_telemetry(tlm, "s")
        e_id = tlm.probe_names()["s.e"]
        probe.steps(x)
        recs = tlm.read()
        e = recs["value"][recs["probe"] == e_id]
        err.append(float(np.mean(e[len(e) // 3 :])))  # drop the fill transient
    return offs, np.asarray(err)


def main(out_path="ratesync_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    settled = symbols[len(symbols) // 3 :]
    evm = evm_self(settled)
    ber = ber_wide(bits, settled)

    fig, axes = plt.subplots(3, 1, figsize=(9, 10))

    ax = axes[0]
    ax.plot(settled.real, settled.imag, ".", ms=2, alpha=0.5)
    ax.set_title(
        f"Recovered constellation at sps = {true_sps:.5f} "
        f"(non-integer) — EVM {20 * np.log10(evm):.1f} dB, BER {ber:.3g}"
    )
    ax.set_xlabel("I")
    ax.set_ylabel("Q")
    ax.grid(alpha=0.3)
    ax.axhline(0, lw=0.5, color="k")
    ax.axvline(0, lw=0.5, color="k")

    ax = axes[1]
    ax.plot(sym_count, rate_trace, color="C0", label="tracked")
    ax.axhline(true_sps, ls="--", color="C1", label=f"true {true_sps:.5f}")
    ax.axhline(SPS, ls=":", color="C7", label=f"nominal {SPS:.5f}")
    ax.set_title(
        "Tracked clock — the loop pulls off the nominal onto the true "
        "non-integer rate"
    )
    ax.set_xlabel("symbols recovered")
    ax.set_ylabel("samples / symbol")
    ax.set_ylim(
        min(true_sps, SPS) - 4 * abs(true_sps - SPS),
        max(true_sps, SPS) + 4 * abs(true_sps - SPS),
    )
    ax.legend(loc="best")
    ax.grid(alpha=0.3)

    ax = axes[2]
    offs, err = s_curve()
    ax.plot(offs, err, "-o", ms=3)
    ax.axhline(0, lw=0.8, color="k")
    zc = offs[np.argmin(np.abs(err))]
    ax.set_title(
        f"Timing S-curve — standard Gardner: one period per symbol, "
        f"odd-symmetric (+{err.max():.2f}/{err.min():.2f})"
    )
    ax.axvline(zc, ls=":", color="C7", lw=0.8)
    ax.set_xlabel("timing offset [symbols]")
    ax.set_ylabel("mean Gardner error")
    ax.grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")

    # ---- self-validation: exit 0 means demonstrated AND checked -----------
    assert rx.locked, "timing loop never declared lock"
    assert evm < 0.30, f"EVM {evm:.3f} too high for {SNR_DB} dB SNR"
    assert ber == 0.0, f"BER {ber:.3g} != 0 on the settled segment"
    assert abs(rx.rate - true_sps) < 1e-3 * SPS, (
        f"tracked rate {rx.rate:.5f} != true {true_sps:.5f}"
    )
    # the tracked rate must be closer to the truth than the nominal it started
    # from — i.e. the loop genuinely moved, it did not just sit at the nominal
    assert abs(rx.rate - true_sps) < abs(SPS - true_sps) / 2, (
        "the loop did not track the clock offset"
    )
    print(
        f"OK  sps(true)={true_sps:.5f} tracked={rx.rate:.5f}  "
        f"EVM={20 * np.log10(evm):.1f} dB  BER={ber:.3g}  locked={rx.locked}"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "ratesync_demo.png")
