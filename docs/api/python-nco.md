# Python Source API — NCO / LO / AWGN

Three signal-source classes in the `doppler.source` module:

| Class  | Output                                         | Use when                                  |
| ------ | ---------------------------------------------- | ----------------------------------------- |
| `LO`   | CF32 complex phasors via 2¹⁶-entry sin/cos LUT | Generate IQ tones, FM signals             |
| `NCO`  | uint32 raw phase accumulator                   | Drive polyphase clock, generate carries   |
| `AWGN` | CF32 complex Gaussian noise                    | Noise injection, SNR testing, Monte Carlo |

Source:
[`src/doppler/source/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/__init__.py)

______________________________________________________________________

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

# One sample (LO is block-oriented; take the first of a length-1 batch)
s = lo.steps(1)[0]

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

______________________________________________________________________

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

______________________________________________________________________

::: doppler.source.LO

______________________________________________________________________

::: doppler.source.NCO

______________________________________________________________________

## `AWGN` — Additive White Gaussian Noise

xoshiro256++ RNG + Box-Muller transform. Per-component std dev = `amplitude`.
AVX-512 path runs 8 independent streams in parallel (~525 MSa/s).

```python
from doppler.source import AWGN
import numpy as np

g = AWGN(seed=42, amplitude=1.0)
noise = g.generate(1024)    # complex64, length 1024

# Amplitude can be changed without disturbing the RNG state
g.amplitude = 0.5

# Deterministic replay
g.reset()
same_noise = g.generate(1024)

# New seed
g.reseed(999)
```

______________________________________________________________________

::: doppler.source.AWGN
