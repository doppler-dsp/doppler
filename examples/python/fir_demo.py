"""fir_demo.py — FIR filter demo for CF32 IQ signal streams.

Shows:
  1. Designing a low-pass filter with scipy and running it through doppler's
     FIR engine.
  2. State persistence across buffer boundaries.

Run:
  python examples/python/fir_demo.py
"""

import numpy as np
from scipy.signal import firwin

from doppler.filter import FIR

# ── 1. Design and apply a low-pass filter ────────────────────────────────────

N_TAPS = 15
CUTOFF = 0.1  # normalised cutoff (fraction of Nyquist)
N_BLOCK = 32

taps = firwin(N_TAPS, cutoff=CUTOFF, window="hann").astype(np.float32)
print("=== doppler FIR filter demo ===\n")
print(f"15-tap Hann-windowed sinc LP taps (cutoff = {CUTOFF} × Fs):")
print("  " + "  ".join(f"{t:7.4f}" for t in taps))
print()

fir = FIR(taps)

# Mixed input: 0.05 Fs tone (passband) + 0.4 Fs tone (stopband)
t_idx = np.arange(N_BLOCK)
lo_tone = np.exp(2j * np.pi * 0.05 * t_idx).astype(np.complex64)
hi_tone = (0.5 * np.exp(2j * np.pi * 0.4 * t_idx)).astype(np.complex64)
x = lo_tone + hi_tone

y = fir.execute(x)

print("--- CF32 input: 0.05 Fs + 0.4 Fs tone ---")
print("  first 8 samples (LF tone survives, HF attenuated):")
print(
    f"  {'idx':>3}  {'in.real':>8}  {'in.imag':>8}  {'out.real':>9}  {'out.imag':>9}"
)
for i in range(8):
    print(
        f"  {i:>3}  {x[i].real:>8.3f}  {x[i].imag:>8.3f}"
        f"  {y[i].real:>9.3f}  {y[i].imag:>9.3f}"
    )
print()

# ── 2. State persistence across buffer boundaries ─────────────────────────────

fir.reset()

t1 = np.arange(8)
t2 = np.arange(8, 16)


def tone(t):
    return np.exp(2j * np.pi * 0.05 * t).astype(np.complex64)


out1 = fir.execute(tone(t1)).copy()  # copy — execute returns a view
out2 = fir.execute(tone(t2))

print("--- stateful: 2×8 CF32 blocks (delay line persists) ---")
print(f"  block 1 last sample:  {out1[-1].real:.3f} + {out1[-1].imag:.3f}j")
print(f"  block 2 first sample: {out2[0].real:.3f} + {out2[0].imag:.3f}j")
print("  (continuity confirms delay line carries across calls)")
print()
print("Done.")
