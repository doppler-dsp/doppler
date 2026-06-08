"""example.py — minimal doppler Python example.

Install:
    pip install doppler-dsp

Run:
    python examples/standalone/example.py
"""

import numpy as np
from doppler.source import awgn

noise = awgn(4096, amplitude=1.0, seed=42)

mean = noise.mean()
std_re = float(np.std(np.real(noise)))
std_im = float(np.std(np.imag(noise)))

print(f"samples : {len(noise)}")
print(f"mean    : {mean.real:.4f} + {mean.imag:.4f}i  (expect ≈ 0)")
print(f"std dev : {std_re:.4f} (Re)  {std_im:.4f} (Im)  (expect ≈ 1.0)")
