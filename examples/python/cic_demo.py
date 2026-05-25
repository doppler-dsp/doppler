"""cic_demo.py — CIC decimation filter demo.

Shows:
  1. Basic decimation — output sample count and DC passthrough
  2. Alias rejection — passband vs alias-zone tones measured in dB
  3. Reconfigure — switch R/N/M at runtime and verify the new rate
  4. SDR front-end pipeline — wideband IQ → CIC → narrowband spectrum

A CIC filter is a cascade of N integrators and N comb sections separated
by R:1 downsampling.  Every stage is an integer accumulator with no
multiplications, making it ideal as the first decimation stage at very
high sample rates (R = 8 … 1024).

Run:
  python examples/python/cic_demo.py
"""

from __future__ import annotations

import math

import numpy as np

from doppler.filter import CIC


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _tone(freq_norm: float, n: int) -> np.ndarray:
    """Complex exponential at freq_norm (cycles/sample)."""
    t = np.arange(n)
    return np.exp(2j * np.pi * freq_norm * t).astype(np.complex64)


def _rms_db(x: np.ndarray) -> float:
    rms = float(np.sqrt(np.mean(np.abs(x) ** 2)))
    return 20.0 * math.log10(rms + 1e-300)


def _decimate(cic: CIC, x: np.ndarray) -> np.ndarray:
    return np.array(cic.decimate(x), copy=True)


def _settled(y: np.ndarray, R: int, N: int) -> np.ndarray:
    """Drop the initial transient (≈ N*(R-1)/R output samples)."""
    n_drop = N * (R - 1) // R + 2
    return y[n_drop:]


# ---------------------------------------------------------------------------
# 1. Basic decimation
# ---------------------------------------------------------------------------

def demo_basic():
    print("--- 1. Basic decimation (R=8, N=4, M=1) ---")
    R, N = 8, 4
    cic = CIC(R, N, 1)

    print(f"  R={cic.R}  N={cic.N}  M={cic.M}")
    print(f"  input_scale  = {cic.input_scale:.3e}")
    print(f"  output_scale = {cic.output_scale:.3e}")

    # DC passthrough: after the transient the output must equal 1.0
    n_in = 12 * R * N
    x = np.ones(n_in, dtype=np.complex64)
    y = _decimate(cic, x)
    settled = _settled(y, R, N)
    dc_rms = float(np.mean(np.abs(settled)))
    print(f"  DC input → settled output magnitude: {dc_rms:.6f} (expect 1.0)")
    print(f"  output samples: {len(y)}  (= {n_in} / {R})\n")


# ---------------------------------------------------------------------------
# 2. Alias rejection
# ---------------------------------------------------------------------------

def demo_alias_rejection():
    print("--- 2. Alias rejection (R=8, N=4, M=1) ---")
    R, N = 8, 4

    # Passband: 10% of output Nyquist → 0.1/(2R) of input fs
    f_pass  = 0.1 / (2 * R)
    # Alias zone: 95% of the way to the first CIC null at fs/R
    f_alias = 0.95 / R

    n_in = 32 * R * N
    x_pass  = _tone(f_pass,  n_in)
    x_alias = _tone(f_alias, n_in)

    cic = CIC(R, N, 1)
    y_pass  = _settled(_decimate(cic, x_pass),  R, N)
    cic.reset()
    y_alias = _settled(_decimate(cic, x_alias), R, N)

    pass_db  = _rms_db(y_pass)
    alias_db = _rms_db(y_alias)
    rejection = pass_db - alias_db

    # Theoretical: 20*N*log10(pi * f_alias * R / sin(pi * f_alias)) for M=1
    sinc_atten = 20.0 * N * math.log10(
        math.pi * f_alias * R / math.sin(math.pi * f_alias * R) + 1e-300
    )
    print(f"  passband tone   f={f_pass:.4f}*fs  → {pass_db:+.1f} dBFS")
    print(f"  alias-zone tone f={f_alias:.4f}*fs  → {alias_db:+.1f} dBFS")
    print(f"  alias rejection: {rejection:.1f} dB")
    print(f"  theoretical:     {sinc_atten:.1f} dB\n")


# ---------------------------------------------------------------------------
# 3. Reconfigure at runtime
# ---------------------------------------------------------------------------

def demo_reconfigure():
    print("--- 3. Runtime reconfigure ---")
    cic = CIC(4, 2, 1)
    print(f"  initial   R={cic.R}  N={cic.N}  M={cic.M}")

    x = np.ones(32, dtype=np.complex64)
    y = cic.decimate(x)
    print(f"  32 samples → {len(y)} output (R=4)")

    cic.reconfigure(8, 4, 1)
    print(f"  reconfigured R={cic.R}  N={cic.N}  M={cic.M}")

    x = np.ones(64, dtype=np.complex64)
    y = cic.decimate(x)
    print(f"  64 samples → {len(y)} output (R=8)\n")


# ---------------------------------------------------------------------------
# 4. SDR front-end pipeline
# ---------------------------------------------------------------------------

def demo_sdr_pipeline():
    """Simulate a wideband IQ signal and decimate it to a narrowband slice.

    Input: 2.048 Msps IQ with two tones
      - wanted signal at fc + 15 kHz  (inside 128 ksps output Nyquist)
      - interference at fc + 600 kHz  (outside, should be aliased away)

    After 16:1 CIC decimation: output at 128 ksps.  The 600 kHz interferer
    folds to 600 - floor(600/128)*128 = 600 - 512 = 88 kHz, which is inside
    the output band — but the CIC has already attenuated it significantly.
    """
    print("--- 4. SDR front-end pipeline ---")

    fs_in  = 2.048e6    # input sample rate
    R      = 16         # CIC decimation
    fs_out = fs_in / R  # 128 ksps

    f_wanted = 15e3     # Hz relative to centre
    f_jammer = 600e3    # Hz — outside output Nyquist (64 kHz)

    fn_wanted = f_wanted / fs_in
    fn_jammer = f_jammer / fs_in

    N_IN  = 8 * R * 12   # enough for settling + measurement
    x = _tone(fn_wanted, N_IN) + 0.5 * _tone(fn_jammer, N_IN)

    print(f"  Input fs = {fs_in/1e6:.3f} Msps, R={R} → output {fs_out/1e3:.0f} ksps")
    print(f"  Wanted:  {f_wanted/1e3:.0f} kHz  (fn={fn_wanted:.5f})")
    print(f"  Jammer:  {f_jammer/1e3:.0f} kHz  (fn={fn_jammer:.5f})")

    cic = CIC(R, 4, 1)
    y = _decimate(cic, x)
    settled = _settled(y, R, 4)

    # RMS with only the wanted tone in the same input
    cic.reset()
    y_ref = _settled(_decimate(cic, _tone(fn_wanted, N_IN)), R, 4)

    wanted_db  = _rms_db(y_ref)
    combined_db = _rms_db(settled)

    print(f"  Wanted-only output RMS:   {wanted_db:+.1f} dBFS")
    print(f"  Combined output RMS:      {combined_db:+.1f} dBFS")
    print(
        f"  Jammer visible as excess: {combined_db - wanted_db:+.1f} dB "
        f"(would be +{20*math.log10(1 + 0.5):.1f} dB without CIC filtering)"
    )
    print()


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("=== doppler CIC decimation filter demo ===\n")
    demo_basic()
    demo_alias_rejection()
    demo_reconfigure()
    demo_sdr_pipeline()
    print("Done.")
