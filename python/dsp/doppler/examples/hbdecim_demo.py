#!/usr/bin/env python3
"""
Halfband decimator demo — 2:1 decimation of a complex tone.

Demonstrates:
  1. Creating a halfband decimator with a simple symmetric filter.
  2. Decimating a complex tone by 2, showing input and output samples.
  3. DC gain near unity, confirming spectral response.

This is the Python port of c/examples/hbdecim_demo.c.
"""

import numpy as np
from doppler.polyphase import kaiser_prototype
from doppler.resample import HalfbandDecimator

N_IN = 32


def rms_db(x):
    """Compute the RMS power of a complex signal in dB."""
    s = np.sum(np.abs(x) ** 2)
    return 10.0 * np.log10(s / len(x) + 1e-20)


def main():
    print("=== Halfband Decimator Demo ===\n")

    # Create a halfband bank using kaiser_prototype with phases=2
    _, bank = kaiser_prototype(phases=2, stopband=0.45)
    dec = HalfbandDecimator(bank)

    print(f"Decimator created: {dec.num_taps} taps, rate={dec.rate:.1f}\n")

    # Generate a complex tone at 0.125 cycles/sample (eighth rate)
    tone_freq = 0.125
    n = np.arange(N_IN)
    x = np.exp(2.0j * np.pi * tone_freq * n).astype(np.complex64)

    print(f"Input: {N_IN} samples at f_n = {tone_freq:.3f}")
    print(f"Input RMS power: {rms_db(x):.2f} dBFS\n")

    # Decimate: expecting ~16 output samples from 32 inputs
    y = dec.execute(x)

    print(f"Output: {len(y)} samples (decimation ratio 2:1)")
    print(f"Output RMS power: {rms_db(y):.2f} dBFS\n")

    # Show first 8 samples
    print("First 8 output samples:")
    print(f"{'idx':<4}  {'I':>10}  {'Q':>10}")
    print("----  ----------  ----------")
    for k in range(min(8, len(y))):
        print(f"{k:<4}  {y[k].real:+10.6f}  {y[k].imag:+10.6f}")


if __name__ == "__main__":
    main()
