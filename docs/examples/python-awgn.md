# AWGN — Additive White Gaussian Noise

`AWGN` generates complex CF32 noise where real and imaginary parts are
independent zero-mean Gaussians with standard deviation `amplitude`.
Total complex power = 2 × amplitude².

Source:
[`src/doppler/source/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/__init__.py)

---

## Basic generation

```python
from doppler.source import AWGN
import numpy as np

g = AWGN(seed=42, amplitude=1.0)
noise = g.generate(1024)      # complex64, length 1024
print(noise.dtype, noise.shape)
# complex64 (1024,)

# Per-component statistics
re = np.real(noise)
print(f"mean={re.mean():.3f}  std={re.std():.3f}")
# mean≈0.000  std≈1.000
```

---

## Amplitude control

`amplitude` sets the per-component standard deviation and can be changed
without disturbing the RNG state:

```python
g = AWGN(seed=0, amplitude=0.5)
n = g.generate(65536)
print(np.std(np.real(n)))     # ≈ 0.500

g.amplitude = 0.1             # retune in-place, RNG continues
n2 = g.generate(65536)
print(np.std(np.real(n2)))    # ≈ 0.100
```

---

## Seeding and reproducibility

Generators with the same seed produce identical output:

```python
a = AWGN(seed=7, amplitude=1.0)
b = AWGN(seed=7, amplitude=1.0)
assert np.array_equal(a.generate(256), b.generate(256))
```

`reset()` rewinds to the state at construction:

```python
g = AWGN(seed=42, amplitude=1.0)
first  = g.generate(1024)
g.reset()
second = g.generate(1024)
assert np.array_equal(first, second)
```

`reseed()` replaces the seed and resets:

```python
g = AWGN(seed=1, amplitude=1.0)
run_a = g.generate(256)

g.reseed(999)
run_b = g.generate(256)
assert not np.array_equal(run_a, run_b)
```

---

## Combining with a signal

Add noise to a carrier from `LO` to model a received signal at a
given SNR:

```python
from doppler.source import AWGN, LO
import numpy as np

N   = 4096
SNR = 10.0          # dB

sig_amp   = 1.0 / np.sqrt(2)                     # per-component (unit complex)
noise_amp = sig_amp / (10 ** (SNR / 20.0))       # per-component noise std dev

lo    = LO(0.1)
awgn  = AWGN(seed=0, amplitude=noise_amp)

carrier = lo.steps(N)
noise   = awgn.generate(N)
rx      = carrier + noise

snr_measured = (np.mean(np.abs(carrier) ** 2)
                / np.mean(np.abs(noise) ** 2))
print(f"SNR: {10 * np.log10(snr_measured):.1f} dB")
# SNR: 10.0 dB
```

---

## Performance

At 64 K blocks, `AWGN.generate()` reaches:

| Path | Rate |
|------|------|
| Scalar (x86) | ~183 MSa/s |
| AVX-512 (8-stream xoshiro + libmvec log) | ~525 MSa/s |

The AVX-512 path is selected automatically at runtime when the CPU
supports it; no user action required.

---

::: doppler.source.AWGN
