"""doppler_channel_demo.py -- clock Doppler is not a frequency offset.

A Doppler shift rescales the whole received time base. The carrier moves,
*and so does every clock in the signal* -- chip rate, symbol rate, frame rate.
Modelling only the carrier is the standard shortcut, and it quietly removes
the one error a delay-lock loop exists to track.

:class:`~doppler.impairment.DopplerChannel` applies both halves from a single
parameter, so they cannot disagree:

- **time-base dilation**, by resampling at output/input ratio ``1/(1+d)``
  (:class:`~doppler.resample.Resampler`'s per-sample rate control, so a
  Doppler *ramp* is tracked exactly rather than approximated per block);
- **carrier offset**, by multiplying by ``exp(j2*pi*fc*excess(t))``.

Doppler is given in **ppm of the nominal time base**, which is what makes it
carrier-frequency agnostic -- one number is simultaneously a shift in Hz and a
rate error in chips/s. ``carrier_hz`` is therefore load-bearing DSP input here,
not the SigMF metadata it is everywhere else in this codebase: it is the only
thing that converts a dimensionless ppm into Hz.

Geometry throughout is ``prototypes/async_despreader/SPEC.md``'s: 3.069 Mcps at
``spc=2`` on a 2.5 GHz carrier. That spec's +/-50 kHz frequency uncertainty and
its 500 Hz/s rate of change are, in ppm, exactly **20 ppm** and **0.2 ppm/s**.

Three panels:

1. **Carrier offset is linear in ppm** -- measured FFT peak against the
   ``fc*d`` prediction, swept across the spec's +/-20 ppm.
2. **The time base dilates** -- accumulated code-phase slip in chips. This is
   the panel a carrier-only model gets wrong: it would be flat at zero, while
   the real channel slips 61.4 chips per second at 20 ppm.
3. **A Doppler ramp is the integral, not ``t*d(t)``** -- offset climbs at
   ``fc*d_dot``. The natural wrong implementation is overlaid; it lands at
   exactly twice the truth.

A note on what is deliberately *not* plotted: under a Doppler *rate* the code
slips quadratically (``Rc*0.5*d_dot*t^2``), but at this spec's 0.2 ppm/s that
is 0.08 chips over the whole half-second run -- a fraction of one sample, and
below what sample counting can even resolve. The carrier effect of a ramp is
first order and plainly visible; the code effect is second order and, over a
realistic dwell, negligible. That asymmetry is why panel 3 tracks the carrier.

Run:  python -m doppler.examples.doppler_channel_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:channel]
import numpy as np

from doppler.impairment import DopplerChannel

# SPEC.md geometry, with its RF numbers restated as ppm of the time base.
CHIP_RATE = 3.069e6  # Mcps
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # 6.138 MS/s
FC = 2.5e9  # RF carrier -- load-bearing, not metadata
PPM = 20.0  # +/-50 kHz at 2.5 GHz
RATE_PPM_S = 0.2  # 500 Hz/s at 2.5 GHz


def apply_doppler(x: np.ndarray, ppm: float, rate_ppm_s: float = 0.0):
    """Push ``x`` through a Doppler channel; return ``(y, channel)``.

    The channel dilates the time base *and* shifts the carrier, both derived
    from ``ppm``. Output is shorter than input by roughly ``ppm`` parts per
    million -- that missing time is the dilation.
    """
    ch = DopplerChannel(
        fs=FS,
        carrier_hz=FC,
        doppler_ppm=ppm,
        doppler_rate_ppm_s=rate_ppm_s,
    )
    return ch.execute(x.astype(np.complex64)), ch


# --8<-- [end:channel]

N = 1 << 16  # one block, ~10.7 ms at this fs


def _dc(n: int = N) -> np.ndarray:
    """DC input: any frequency content in the output came from the channel."""
    return np.ones(n, dtype=np.complex64)


def _peak_hz(y: np.ndarray) -> float:
    """Dominant frequency of ``y``, by FFT peak."""
    sp = np.abs(np.fft.fft(y))
    f = np.fft.fftfreq(len(y), 1.0 / FS)
    return float(f[int(np.argmax(sp))])


def sweep_offset(ppms):
    """Measured carrier offset at each ppm, alongside the ``fc*d`` theory."""
    meas, theory = [], []
    for p in ppms:
        y, _ = apply_doppler(_dc(), p)
        meas.append(_peak_hz(y))
        theory.append(FC * p * 1e-6)
    return np.array(meas), np.array(theory)


def slip_trace(ppm: float, n_blocks: int = 24):
    """Accumulated code-phase slip in chips, block by block.

    Every sample the dilation removes is a chip the receiver's code loop has
    to make up. Theory is ``Rc*d*t`` chips after ``t`` seconds.
    """
    ch = DopplerChannel(fs=FS, carrier_hz=FC, doppler_ppm=ppm)
    x = _dc()
    t, slip = [0.0], [0.0]
    n_in = 0
    n_out = 0
    for _ in range(n_blocks):
        n_out += len(ch.execute(x))
        n_in += len(x)
        t.append(n_out / FS)
        slip.append((n_in - n_out) / SPC)  # samples -> chips
    return np.array(t), np.array(slip)


def ramp_trace(rate_ppm_s: float, n_blocks: int = 48):
    """Offset and code slip vs time under a pure Doppler *rate*.

    Returns ``(t, offset_hz, slip_chips)``. The offset is linear in ``t``
    (the integral of a linear rate), while the code slip -- being the
    integral of that same offset in the code's own units -- is quadratic.
    """
    ch = DopplerChannel(
        fs=FS, carrier_hz=FC, doppler_ppm=0.0, doppler_rate_ppm_s=rate_ppm_s
    )
    x = _dc()
    t, off, slip = [0.0], [0.0], [0.0]
    n_in = 0
    n_out = 0
    for _ in range(n_blocks):
        n_out += len(ch.execute(x))
        n_in += len(x)
        t.append(ch.elapsed_s)
        off.append(ch.offset_hz)
        slip.append((n_in - n_out) / SPC)
    return np.array(t), np.array(off), np.array(slip)


def main(out_path: str = "doppler_channel_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # --- panel 1: offset is linear in ppm --------------------------------
    ppms = np.linspace(-PPM, PPM, 9)
    meas, theory = sweep_offset(ppms)
    bin_hz = FS / N
    assert np.max(np.abs(meas - theory)) < 3 * bin_hz, (
        "measured carrier offset should track fc*d to within a few FFT bins "
        f"(worst {np.max(np.abs(meas - theory)):.1f} Hz, bin {bin_hz:.1f} Hz)"
    )

    # --- panel 2: the time base dilates ----------------------------------
    t_slip, slip = slip_trace(PPM)
    slip_theory = CHIP_RATE * PPM * 1e-6 * t_slip
    assert slip[-1] > 0.0, "positive Doppler must compress the time base"
    # Slip is counted in whole samples, so it is quantised to 1/spc chips;
    # allow that floor alongside the proportional tolerance rather than
    # relying on the two happening to land close.
    slip_tol = max(0.02 * slip_theory[-1], 1.0 / SPC)
    assert abs(slip[-1] - slip_theory[-1]) < slip_tol, (
        f"code slip {slip[-1]:.2f} chips should match Rc*d*t "
        f"{slip_theory[-1]:.2f} chips (tol {slip_tol:.2f})"
    )

    # --- panel 3: the ramp is the integral -------------------------------
    t_ramp, off, _ramp_slip = ramp_trace(RATE_PPM_S)
    off_theory = FC * RATE_PPM_S * 1e-6 * t_ramp  # 500 Hz/s
    off_wrong = 2.0 * off_theory  # what t*d(t) would give
    assert np.allclose(off, off_theory, rtol=1e-6), (
        "Doppler rate must integrate to fc*d_dot*t"
    )
    assert not np.allclose(off[1:], off_wrong[1:], rtol=1e-2), (
        "offset must not double-count the ramp"
    )

    print(
        f"offset @ {PPM:.0f} ppm : {meas[-1]:9.1f} Hz  "
        f"(theory {theory[-1]:.1f})\n"
        f"code slip @ {t_slip[-1] * 1e3:.1f} ms: {slip[-1]:7.2f} chips  "
        f"(theory {slip_theory[-1]:.2f})\n"
        f"ramp slope        : {FC * RATE_PPM_S * 1e-6:9.1f} Hz/s"
    )

    # --- plot -------------------------------------------------------------
    fig, (a, b, c) = plt.subplots(1, 3, figsize=(15, 4.6))

    a.plot(ppms, theory * 1e-3, "k--", lw=1.2, label="fc·d (theory)")
    a.plot(ppms, meas * 1e-3, "o", color="#1f77b4", ms=5, label="measured")
    a.set_title(
        "Carrier offset is linear in ppm\n"
        f"fc = {FC / 1e9:.1f} GHz → {PPM:.0f} ppm = "
        f"{FC * PPM * 1e-6 / 1e3:.0f} kHz",
        fontsize=9,
    )
    a.set_xlabel("Doppler (ppm)")
    a.set_ylabel("measured offset (kHz)")
    a.legend(fontsize=7)
    a.grid(alpha=0.25)

    b.plot(t_slip * 1e3, slip, lw=1.4, color="#d62728", label="DopplerChannel")
    b.plot(t_slip * 1e3, slip_theory, "k--", lw=1.0, label="Rc·d·t (theory)")
    b.axhline(
        0.0,
        color="#7f7f7f",
        lw=1.2,
        ls=":",
        label="carrier-only model",
    )
    b.set_title(
        "The time base dilates, not just the carrier\n"
        f"{PPM:.0f} ppm → {CHIP_RATE * PPM * 1e-6:.1f} chips/s of slip",
        fontsize=9,
    )
    b.set_xlabel("time (ms)")
    b.set_ylabel("accumulated code slip (chips)")
    b.legend(fontsize=7)
    b.grid(alpha=0.25)

    c.plot(t_ramp * 1e3, off, lw=1.4, color="#2ca02c", label="DopplerChannel")
    c.plot(
        t_ramp * 1e3,
        off_theory,
        "k--",
        lw=1.0,
        label="∫d dt = fc·ḋ·t (theory)",
    )
    c.plot(
        t_ramp * 1e3,
        off_wrong,
        ":",
        color="#ff7f0e",
        lw=1.2,
        label="t·d(t) — double-counts",
    )
    c.set_title(
        "A Doppler ramp is the integral of the rate\n"
        f"{RATE_PPM_S} ppm/s → {FC * RATE_PPM_S * 1e-6:.0f} Hz/s",
        fontsize=9,
    )
    c.set_xlabel("time (ms)")
    c.set_ylabel("instantaneous offset (Hz)")
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

    fig.suptitle(
        "DopplerChannel — clock Doppler as a propagation impairment "
        f"(Rc={CHIP_RATE / 1e6:.3f} Mcps, spc={SPC}, fc={FC / 1e9:.1f} GHz)",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.90))
    fig.subplots_adjust(wspace=0.28)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "doppler_channel_demo.png")
