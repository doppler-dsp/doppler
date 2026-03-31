#!/usr/bin/env python3
"""
NCO demo — free-running tone and FM modulation.

Demonstrates:
  1. Free-running NCO at f_n = 0.25 (quarter sample rate).
     The I/Q samples rotate 90° per sample, cycling through
     (1,0) → (0,1) → (-1,0) → (0,-1) → ...

  2. FM-modulated NCO: a sine-wave modulation signal applied to
     the phase-increment control port.

This is the Python port of c/examples/nco_demo.c.
"""

import numpy as np
from doppler import Nco

N_FREE = 16  # free-running samples to print
N_FM = 32  # FM-modulated samples to generate
FM_RATE = 0.25  # modulating tone: f_n of the sine wave
FM_DEV = 0.05  # peak frequency deviation (normalised)


def main():
    # ===================================================================
    # 1. Free-running quarter-rate NCO
    # ===================================================================
    print("=== Free-running NCO  (f_n = 0.25) ===")
    print(f"{'sample':<6}  {'I':>9}  {'Q':>9}")
    print("------  ---------  ---------")

    nco = Nco(0.25)
    out = nco.execute_cf32(N_FREE)

    for i in range(N_FREE):
        print(f"{i:<6}  {out[i].real:+9.6f}  {out[i].imag:+9.6f}")

    # ===================================================================
    # 2. FM-modulated NCO
    #
    # Carrier: f_n = 0.1
    # Modulator: sine wave at FM_RATE, amplitude FM_DEV
    #   → instantaneous frequency sweeps 0.1 ± 0.05
    # ===================================================================
    print(f"\n=== FM NCO  (carrier 0.10, dev ±{FM_DEV:.2f}, mod {FM_RATE:.2f}) ===")
    print(f"{'sample':<6}  {'I':>9}  {'Q':>9}  {'f_inst':>9}")
    print("------  ---------  ---------  ---------")

    ctrl = (FM_DEV * np.sin(2.0 * np.pi * FM_RATE * np.arange(N_FM))).astype(np.float32)
    fm = Nco(0.1)
    fmo = fm.execute_cf32_ctrl(ctrl)

    for i in range(N_FM):
        f_inst = 0.1 + ctrl[i]
        print(f"{i:<6}  {fmo[i].real:+9.6f}  {fmo[i].imag:+9.6f}  {f_inst:+9.6f}")


if __name__ == "__main__":
    main()
