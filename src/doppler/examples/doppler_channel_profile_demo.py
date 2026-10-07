"""doppler_channel_profile_demo.py -- one cosine period drives the Doppler.

``DopplerChannel(doppler_ppm=..., doppler_rate_ppm_s=...)`` is a straight
line in time. A real pass is not one: a satellite's range rate swings from
closing to opening and back, so the Doppler it imposes is a *curve*.
``execute_profile`` takes that curve as an array, one ppm value per input
sample, and applies it.

Here the control is **one full period of a cosine**,

    d(t) = A * cos(2*pi * t / T),    A = 20 ppm,  T = 0.25 s,

which starts closing at +20 ppm, passes through zero, opens to -20 ppm and
comes back. It is the simplest curve no ``(d0, d_dot)`` can express -- the best
straight line through a whole period is flat, and explains none of it -- and it
integrates in closed form, so everything the channel does can be checked
against an exact answer rather than against a second implementation:

* the **carrier offset** is ``fc * d(t)``: a cosine of +/-50 kHz at 2.5 GHz;
* the **time base** dilates by ``excess(t) = A/w * sin(w t)``, so the code
  slips by ``Rc * excess(t)`` chips and *returns to zero* when the period
  ends;
* so a whole period leaves the stream exactly as long as it started, and the
  carrier phase back where it began.

Geometry is ``docs/design/async-dsss-receiver.md``'s: 3.069 Mcps at
``spc=8`` on a 2.5 GHz carrier, whose +/-50 kHz uncertainty is, in ppm,
exactly +/-20.

The profile is **absolute** (the channel is created with zeros and the array is
the whole Doppler) and the stream is fed in blocks, because the result must not
depend on how it was chunked: the example asserts that the blocks and one call
are the same samples to the bit.

Run:  python -m doppler.examples.doppler_channel_profile_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:profile]
import numpy as np

from doppler.impairment import DopplerChannel

CHIP_RATE = 3.069e6  # Mcps
SPC = 8  # samples per chip
FS = CHIP_RATE * SPC  # 24.552 MS/s
FC = 2.5e9  # RF carrier -- load-bearing, not metadata
AMP_PPM = 20.0  # +/-50 kHz at 2.5 GHz
PERIOD_S = 0.25  # one cosine period
N = round(FS * PERIOD_S)  # input samples in that one period
BLOCK = 1 << 18  # fed in blocks: the answer must not depend on this
BLOCK_B = 99_991  # a second, unrelated size: not a divisor of the first


def cosine_profile(n: int, amp_ppm: float) -> np.ndarray:
    """One full period of ``amp_ppm * cos`` over ``n`` input samples."""
    return amp_ppm * np.cos(2.0 * np.pi * np.arange(n) / n)


def drive(x: np.ndarray, ppm: np.ndarray, block: int = BLOCK):
    """Run ``x`` through the channel under ``ppm``, ``block`` at a time.

    The channel is created with no Doppler of its own: a profile is absolute,
    so the array is the whole story. Returns the output and, after each block,
    the (input, output) sample counts consumed so far.
    """
    ch = DopplerChannel(fs=FS, carrier_hz=FC)
    ys, counts = [], []
    n_in = n_out = 0
    for i in range(0, len(x), block):
        y = ch.execute_profile(x[i : i + block], ppm[i : i + block])
        ys.append(y)
        n_in += min(block, len(x) - i)
        n_out += len(y)
        counts.append((n_in, n_out))
    return np.concatenate(ys), np.array(counts)


# --8<-- [end:profile]


_SETTLE = 64  # output samples dropped while the resampler's filter fills


def _offset_hz(y: np.ndarray, win: int = 1 << 14):
    """Carrier offset against time: the phase's slope, averaged over ``win``.

    The input is DC, so the output's phase is the channel's alone. Averaging
    the per-sample phase step over a window leaves the offset the window is
    centred on and drops the float32 noise a bare difference would carry.
    """
    # The resampler's filter starts empty, so |y| ramps from 0 to 1 over the
    # first ~2x its group delay (about 20 samples) and the phase of those
    # near-zero samples is noise, not carrier. Drop them; 64 is a margin.
    ph = np.unwrap(np.angle(y[_SETTLE:]).astype(np.float64))
    step = np.diff(ph) * FS / (2.0 * np.pi)  # Hz, per output sample
    m = len(step) // win
    f = step[: m * win].reshape(m, win).mean(axis=1)
    t = (_SETTLE + (np.arange(m) + 0.5) * win) / FS
    return t, f


def main(out_path: str = "doppler_channel_profile_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x = np.ones(
        N, dtype=np.complex64
    )  # DC: the channel's effect is the output
    ppm = cosine_profile(N, AMP_PPM)
    w = 2.0 * np.pi / PERIOD_S

    y, counts = drive(x, ppm)

    # --- the stream does not depend on how it was fed ----------------------
    whole = DopplerChannel(fs=FS, carrier_hz=FC).execute_profile(x, ppm)
    assert np.array_equal(y, whole), "blocks and one call must be the same"
    # Block-size dependence is exactly what the first attempt at this had: a
    # second, unrelated size, and the same stream again.
    y_b, _ = drive(x, ppm, BLOCK_B)
    assert np.array_equal(y, y_b), "two block sizes must be the same stream"

    # --- no straight line explains it --------------------------------------
    t_in = np.arange(N) / FS
    slope, icept = np.polyfit(t_in, ppm, 1)
    resid = ppm - (slope * t_in + icept)
    unexplained = np.sqrt(np.mean(resid**2) / np.mean(ppm**2))
    assert unexplained > 0.99, (
        f"best (d0, d_dot) line explains {1 - unexplained**2:.1%} of a period"
    )

    # --- the carrier offset is fc * d(t) -----------------------------------
    t_f, f_meas = _offset_hz(y)
    f_theory = FC * AMP_PPM * 1e-6 * np.cos(w * t_f)
    f_err = np.max(np.abs(f_meas - f_theory))
    f_peak = FC * AMP_PPM * 1e-6
    # Measured ~1.4 Hz of 50 kHz: what is left is the theory's argument. It is
    # evaluated at the RECEIVE time and the profile is indexed by the input
    # (emission) clock, which differ by the excess delay, ~1e-5 relative.
    # 5e-5 of the peak is 2.5 Hz, so a 0.05% error in the carrier (25 Hz)
    # fails; a looser gate would pass an error 35x the measurement.
    assert f_err < 5e-5 * f_peak, (
        f"offset off its fc*d(t) by {f_err:.1f} Hz of {f_peak:.0f}"
    )

    # --- the time base dilates, and a whole period gives it back -----------
    n_in, n_out = counts[:, 0], counts[:, 1]
    slip = (n_in - n_out) / SPC  # chips of accumulated code slip
    slip_theory = CHIP_RATE * AMP_PPM * 1e-6 / w * np.sin(w * n_in / FS)
    # Slip is counted in whole samples, so one sample (1/spc chip) is the
    # floor; two is the margin.
    assert np.max(np.abs(slip - slip_theory)) < 2.0 / SPC, (
        "code slip must follow Rc * excess(t)"
    )
    assert abs(len(y) - N) <= 2, (
        f"a full period is net-zero dilation, but {N - len(y)} samples differ"
    )

    print(
        f"carrier offset peak  : {np.max(np.abs(f_meas)):9.1f} Hz "
        f"(theory {f_peak:.1f})\n"
        f"offset vs fc*d(t)    : worst {f_err:9.2f} Hz\n"
        f"code slip peak       : {np.max(np.abs(slip)):9.2f} chips "
        f"(theory {np.max(np.abs(slip_theory)):.2f})\n"
        f"stream length        : {len(y)} out for {N} in "
        f"(net dilation of one period: {N - len(y):+d} samples)\n"
        f"best straight line   : leaves {unexplained:.1%} of the curve "
        f"unexplained"
    )

    # --- plot ---------------------------------------------------------------
    fig, (a, b, c) = plt.subplots(1, 3, figsize=(15, 4.6))

    a.plot(t_in * 1e3, ppm, lw=1.6, color="#1f77b4", label="the control")
    a.plot(
        t_in[::4096] * 1e3,
        (slope * t_in + icept)[::4096],
        "k--",
        lw=1.2,
        label="best (d0, d_dot) straight line",
    )
    a.set_title(
        "One cosine period drives the control\n"
        f"no straight line explains it ({unexplained:.1%} left over)",
        fontsize=9,
    )
    a.set_xlabel("time (ms)")
    a.set_ylabel("Doppler (ppm)")
    a.legend(fontsize=7, loc="lower left")
    a.grid(alpha=0.25)

    b.plot(
        t_f * 1e3,
        f_theory * 1e-3,
        "k--",
        lw=1.2,
        label="fc·d(t) (theory)",
    )
    b.plot(
        t_f * 1e3,
        f_meas * 1e-3,
        ".",
        color="#2ca02c",
        ms=4,
        label="measured",
    )
    b.set_title(
        "The carrier follows the curve\n"
        f"±{f_peak / 1e3:.0f} kHz at fc = {FC / 1e9:.1f} GHz",
        fontsize=9,
    )
    b.set_xlabel("time (ms)")
    b.set_ylabel("carrier offset (kHz)")
    b.legend(fontsize=7, loc="lower left")
    b.grid(alpha=0.25)

    c.plot(
        n_in / FS * 1e3,
        slip_theory,
        "k--",
        lw=1.2,
        label="Rc·∫d dt (theory)",
    )
    c.plot(
        n_in / FS * 1e3,
        slip,
        "o",
        color="#d62728",
        ms=4,
        label="DopplerChannel",
    )
    c.axhline(
        0.0,
        color="#7f7f7f",
        lw=1.2,
        ls=":",
        label="carrier-only model",
    )
    c.set_title(
        "The time base dilates, and gives it back\n"
        f"peak slip {np.max(np.abs(slip_theory)):.1f} chips, "
        "zero after a full period",
        fontsize=9,
    )
    c.set_xlabel("time (ms)")
    c.set_ylabel("accumulated code slip (chips)")
    c.legend(fontsize=7, loc="lower left")
    c.grid(alpha=0.25)

    fig.suptitle(
        "DopplerChannel.execute_profile — a cosine period as the Doppler "
        f"(Rc={CHIP_RATE / 1e6:.3f} Mcps, spc={SPC}, fc={FC / 1e9:.1f} GHz)",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.90))
    fig.subplots_adjust(wspace=0.28)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(
        sys.argv[1]
        if len(sys.argv) > 1
        else "doppler_channel_profile_demo.png"
    )
