# AGC — Automatic Gain Control

`AGC` is a closed-loop power controller: it measures output power with an
exponential moving average, computes the error from a target level, and
drives a first-order loop filter to correct the gain. The `decim`
parameter runs the detector and loop update once per `decim`-sample chunk
rather than every sample — coarser timing, same steady-state, lower CPU
cost.

The key design: `agc_steps()` rescales the loop coefficients by `decim`
so that `loop_bw` keeps its per-sample meaning regardless of decimation
factor. All three decimation settings below converge on identical
trajectories — they differ only in how often the loop ticks, not where it
ends up.

![AGC decimated-loop convergence](../assets/agc_convergence.png)

The 20 dB power step at sample 3000 is tracked within 1 dB in ~350 samples
at `loop_bw = 0.00125` for all three decimation settings.

```python
from doppler.agc import AGC
import numpy as np

# 6000-sample tone that steps from −10 dBm to +10 dBm at sample 3000
n = np.arange(6000)
amp = np.where(n < 3000, 10**(-10/20), 10**(10/20))
x = (amp * np.exp(2j * np.pi * 0.02 * n)).astype(np.complex64)

agc = AGC(ref_db=0.0, loop_bw=0.00125, alpha=0.02)
agc.decim = 8          # update loop every 8 samples
y = agc.steps(x)       # normalised output, power → 0 dBm

print(f"commanded gain : {agc.gain_db:+.2f} dB")
print(f"applied gain   : {agc.applied_gain_db:+.2f} dB")
```

```text
commanded gain : -10.00 dB
applied gain   : -10.00 dB
```

```bash
python src/doppler/examples/agc_demo.py   # → agc_convergence.png
```
