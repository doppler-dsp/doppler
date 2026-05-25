# Python Resample API

Three resampler implementations backed by the native C library — all accept
and return `complex64` NumPy arrays with state preserved across calls.

Source:
[`src/doppler/resample/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/resample/__init__.py)

---

## Which class to use

| Class | Algorithm | Rate | Best for |
|-------|-----------|------|----------|
| `Resampler` | Polyphase (4096 phases × 19 taps) | any | General rate conversion |
| `HalfbandDecimator` | Halfband 2:1 CF32 | 0.5 (fixed) | First stage in a DDC chain |
| `HalfbandDecimatorDp` | Halfband 2:1 CF64 | 0.5 (fixed) | Double-precision DDC chain |
| `HalfbandDecimatorR2C` | Halfband 2:1 F32→CF32 | 0.5 (fixed) | Real ADC input → complex baseband |
| `CIC` | Cascaded integrator-comb | 1/R (fixed) | High-rate first decimation stage |

---

## `Resampler` — general polyphase

Built-in Kaiser bank (60 dB rejection, 0.4/0.6 pass/stop).  Works for
any rate — integer, fractional, and irrational.

```python
from doppler.resample import Resampler
import numpy as np

x = np.random.randn(4096).astype(np.complex64)

# Decimate 2×
r = Resampler(0.5)
y = r.execute(x)           # len(y) ≈ 2048

# Interpolate 3×
r2 = Resampler(3.0)
y2 = r2.execute(x)         # len(y2) ≈ 12288

# Fractional — irrational rate is fine
r3 = Resampler(44100 / 48000)
y3 = r3.execute(x)

# Custom Kaiser spec (tighter transition band)
r4 = Resampler(0.5, rejection=80.0, passband=0.35, stopband=0.45)
```

### Rate-controlled resampling (FM/Doppler correction)

Per-sample rate deviation via `execute_ctrl`:

```python
ctrl = np.zeros(4096, dtype=np.complex64)  # deviation in norm_freq units
ctrl.real = 1e-4 * doppler_correction
y = r.execute_ctrl(x, ctrl)
```

---

## `HalfbandDecimator` — fixed 2:1 decimation

Symmetric FIR halfband filter; every other output sample is the identity
(zero multiply) which halves the compute cost vs. a general FIR.
Use as the first stage in a multi-stage DDC chain.

```python
from doppler.resample import HalfbandDecimator
import numpy as np

decim = HalfbandDecimator()          # built-in Kaiser prototype
x = np.random.randn(4096).astype(np.complex64)
y = decim.execute(x)                 # len(y) = 2048
```

Phase-continuous across blocks:

```python
for block in iq_stream:              # 4096-sample CF32 arrays
    y = decim.execute(block)
    next_stage(y)
```

---

---

## `CIC` — cascaded integrator-comb decimator

Fixed-rate integer decimation (factor R, order N).  Runs directly on the
input stream at full rate — no multipliers, just integrators and combs.
Pair with `ciccompmf` to correct passband droop.

```python
from doppler.resample import CIC, ciccompmf
import numpy as np

cic = CIC(R=16, N=4)
x = np.random.randn(4096).astype(np.complex64)
y = cic.execute(x)              # len(y) = 256

# Design a 7-tap droop compensator (runs at output rate)
h = ciccompmf(N=4, R=16, M=7)  # NDArray[float64], length 7
```

---

## `ciccompmf` — CIC droop-compensator design

Closed-form maximally-flat FIR compensator (Molnar & Vucic, IEEE TCAS-II
58(12):926–930, 2011).  Returns a symmetric FIR kernel that corrects CIC
passband droop; apply it at the decimated output rate.

```python
from doppler.resample import ciccompmf

h = ciccompmf(N=4, R=16, M=7)
# h is NDArray[float64] of length M=7, DC gain ≈ 1.0
```

Valid M: odd 1–19, even 2–18.  Out-of-range M returns all-zeros.

---

::: doppler.resample
    options:
      members:
        - Resampler
        - HalfbandDecimator
        - HalfbandDecimatorDp
        - HalfbandDecimatorR2C
        - CIC
        - ciccompmf
