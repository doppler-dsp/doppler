# Python NCO / LO API

Two signal-source classes backed by `nco_state_t` and `lo_state_t`:

| Class | Output | Use when |
|-------|--------|----------|
| `LO` | CF32 complex phasors via 2¹⁶-entry sin/cos LUT | Generate IQ tones, FM signals |
| `NCO` | uint32 raw phase accumulator | Drive polyphase clock, generate carries |

Source:
[`src/doppler/source/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/__init__.py)

---

## `LO` — complex phasor generator

96 dBc SFDR from 16-bit phase truncation into the 65 536-entry LUT.

```python
from doppler.source import LO
import numpy as np

lo = LO(0.25)              # normalised frequency: 0.25 → Fs/4

# Batch generate
iq = lo.steps(1024)        # complex64, length 1024
print(iq[:4])
# [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j ]

# Scalar (one sample at a time)
s = lo.step()

# FM control port — per-sample frequency deviation
ctrl = (0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024))).astype(np.float32)
iq_fm = lo.steps_ctrl(ctrl)

# Retune without resetting phase
lo.norm_freq = 0.1
```

### Phase continuity

```python
lo = LO(0.25)
a = lo.steps(512)
b = lo.steps(512)   # seamlessly continues from sample 512
```

---

## `NCO` — raw phase accumulator

Useful for generating sample-clock events (overflow carries) that drive
a polyphase resampler.

```python
from doppler.source import NCO
import numpy as np

nco = NCO(0.25)

# Raw 32-bit phase values
ph = nco.steps_u32(16)

# Overflow carry — 1 at each wrap (every 4 samples for 0.25)
carry = nco.steps_u32_ovf(16)
# carry: [0, 0, 0, 1, 0, 0, 0, 1, ...]

# Scaled to [0, nmax) — fixed-point multiply, no division
nco2 = NCO(0.25, nmax=1000)
scaled = nco2.steps_u32_scaled(16)   # values in [0, 1000)
```

---

::: doppler.source.LO

---

::: doppler.source.NCO
