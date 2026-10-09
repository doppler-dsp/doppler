# LO — Complex Phasor Generator

`LO` chains a 32-bit NCO with a 2¹⁶-entry sin/cos LUT to produce CF32 IQ
phasors. `freq` is a normalised frequency in cycles/sample (0.25 = fs/4).

## Free-running tone

```python
from doppler.source import LO
import numpy as np

lo = LO(0.25)          # quarter-rate tone
iq = lo.steps(8)

# The 16-bit LUT leaves ~1e-8 residue on the axes, so compare with a tolerance.
assert np.allclose(iq, np.tile([1, 1j, -1, -1j], 2), atol=1e-6)
```

!!! note "CF32 precision"

    The LUT is 16-bit, so values like `0+1j` appear as
    `-8.7e-08+1.000000e+00j` in `repr()`. The absolute error is
    < 1.6 × 10⁻⁵ radians, well below −96 dBc.

## FM via control port

Per-sample frequency deviation added on top of the base frequency:

```python
ctrl = (0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024))).astype(np.float32)
lo2 = LO(0.1)
iq = lo2.steps_ctrl(ctrl)
```

## Phase continuity

Successive calls resume exactly where the last left off:

```python
lo3 = LO(0.25)
a = lo3.steps(4)   # samples 0–3
b = lo3.steps(4)   # samples 4–7, phase continuous

expected = np.tile([1, 1j, -1, -1j], 2)
assert np.allclose(np.concatenate([a, b]), expected, atol=1e-6)
```
