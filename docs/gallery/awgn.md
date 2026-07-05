# AWGN Generator

![AWGN demo](../assets/awgn_demo.png)

## What you're seeing

**Top panel — amplitude histogram.** Real and imaginary components of 65 536
CF32 samples with `amplitude=1.0` overlaid with the theoretical
N(0, σ²=1) Gaussian PDF. Both components track the curve to within
statistical noise, confirming the Box-Muller transform is unbiased.

**Middle panel — Welch PSD.** One-sided power spectral density of
65 536 samples (Re²+Im², `nperseg=1024`). The trace stays within ±1 dB
of the expected floor across the full bandwidth, confirming the spectrum
is white — no tonal artefacts from the LUT phase quantisation.

**Bottom panel — noisy carrier.** Real part of `LO(0.1) + AWGN(σ=0.3)`,
first 256 samples. The clean carrier (dashed) rides underneath the noise.
Total complex power is split evenly: carrier ≈ 1/√2 per component, noise
σ=0.3 per component, giving ≈ 10 dB SNR.

## How it works

```python
from doppler.source import AWGN

g = AWGN(seed=42, amplitude=1.0)
noise = g.generate(65536)   # complex64 array
```

Each complex output sample consumes two 64-bit xoshiro256++ words:

```
u1 = (top-24-bits(word0) + 1) × 2⁻²⁴  ∈ (0, 1]   — Box-Muller uniform
idx = top-16-bits(word1)                           — 65 536-entry LUT index
r   = amplitude × sqrt(−2 × ln u1)                — Box-Muller radius
out = r × cos(idx) + j × r × sin(idx)             — complex Gaussian
```

`amplitude` is the per-component standard deviation.
Total complex power = 2 × amplitude².

The AVX-512 path runs 8 independent xoshiro256++ streams in parallel,
uses glibc libmvec `_ZGVdN8v_logf` for 8-wide vectorised log, and reads
sin/cos from the same 65 536-entry LUT as `LO` via AVX gather
instructions — reaching **~525 MSa/s** on a single AVX-512 core.

```bash
python src/doppler/examples/awgn_demo.py   # → docs/assets/awgn_demo.png
```

## Using the Python API

For a stateless one-shot, call the functional interface; use the stateful
`AWGN` object when you need phase-continuous streams, reproducible replay,
or per-call amplitude changes:

```python
from doppler.source import awgn, AWGN

noise = awgn(1024, amplitude=0.3, seed=42)   # one-shot, no state
g = AWGN(seed=42, amplitude=1.0)             # stateful stream
```

`amplitude` is the per-component standard deviation and retunes in place
without disturbing the RNG state; `reset()` rewinds to construction and
`reseed(s)` replaces the seed and resets:

```python
g.amplitude = 0.1        # retune in-place, RNG continues
g.reset()                # replay from construction
g.reseed(999)            # new seed + reset
```

To model a received signal at a target SNR, set the noise std dev relative
to the signal's per-component amplitude:

```python
import numpy as np
from doppler.source import AWGN, LO

N, SNR = 4096, 10.0                              # dB
sig_amp   = 1.0 / np.sqrt(2)                     # per-component (unit complex)
noise_amp = sig_amp / (10 ** (SNR / 20.0))       # per-component noise std dev

rx = LO(0.1).steps(N) + AWGN(seed=0, amplitude=noise_amp).generate(N)
```

**C one-shot** (no persistent state):

```c
float complex out[1024];
awgn(0, 1.0f, 1024, out);   /* seed=0, amplitude=1.0; returns 0 on success */
```

**C stateful** (streaming / replay):

```c
awgn_state_t *g = awgn_create(42, 1.0f);
awgn_generate(g, 1024, out);
awgn_destroy(g);
```

See [`doppler.source.AWGN`](../api/python-nco.md#awgn-additive-white-gaussian-noise)
for the Python API reference, and [`examples/c`](../examples/c.md#awgn-additive-white-gaussian-noise)
for the full C examples. `wfmgen`'s per-segment noise floors are built on this
same primitive — see [Guide: Scenes → Mixing sources](../guide/wfmgen/scenes.md#mixing-sources-sum-and-sequencing-them-add).
