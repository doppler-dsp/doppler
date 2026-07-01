# Python Resample API

Three resampler implementations backed by the native C library — all accept
and return `complex64` NumPy arrays with state preserved across calls.

Source:
[`src/doppler/resample/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/resample/__init__.py)

______________________________________________________________________

## Which class to use

| Class                  | Algorithm                         | Rate        | Best for                              |
| ---------------------- | --------------------------------- | ----------- | ------------------------------------- |
| `RateConverter`        | Auto-selected cascade             | any         | Single-class interface for all rates  |
| `Resampler`            | Polyphase (4096 phases × 19 taps) | any         | Custom Kaiser spec or `execute_ctrl`  |
| `HalfbandDecimator`    | Halfband 2:1 CF32                 | 0.5 (fixed) | First stage in a hand-tuned DDC chain |
| `HalfbandDecimatorDp`  | Halfband 2:1 CF64                 | 0.5 (fixed) | Double-precision DDC chain            |
| `HalfbandDecimatorR2C` | Halfband 2:1 F32→CF32             | 0.5 (fixed) | Real ADC input → complex baseband     |
| `CIC`                  | Cascaded integrator-comb          | 1/R (fixed) | High-rate first decimation stage      |

______________________________________________________________________

## `RateConverter` — automatic cascade

Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase
Resampler stages for the requested rate ratio at construction time. The
cascade is rebuilt transparently whenever `rate` is changed.

### Stage selection

| Condition (D = 1/rate)  | Cascade                                 |
| ----------------------- | --------------------------------------- |
| rate ≥ 1.0 or D < 2     | `Resampler(rate)`                       |
| D ≈ 2                   | `HalfbandDecimator`                     |
| D ≈ 4                   | `HalfbandDecimator → HalfbandDecimator` |
| D = 2ⁿ, n ≥ 3, D ≤ 4096 | `CIC(D)`                                |
| D ≥ 8, non-power-of-2   | `CIC(R*) → Resampler(R*/D)`             |
| 2 ≤ D < 8, non-integer  | `Resampler(rate)`                       |

where R\* = nearest power-of-two to D.

```python
from doppler.resample import RateConverter
import numpy as np

x = np.random.randn(4096).astype(np.complex64)

rc = RateConverter(0.5)        # HalfbandDecimator
y = rc.execute(x)              # len(y) = 2048

rc = RateConverter(0.125)      # CIC(8)
y = rc.execute(x)              # len(y) = 512

rc = RateConverter(0.1)        # CIC(8) → Resampler(0.8)
y = rc.execute(x)              # len(y) ≈ 410
print(rc.stages)               # ['CIC(8)', 'Resampler(0.8)']

# Change rate — cascade rebuilt, filter state reset
rc.rate = 0.25
print(rc.stages)               # ['HalfbandDecimator', 'HalfbandDecimator']
```

### Streaming

State is preserved across `execute()` calls, so splitting a stream at any block
boundary is byte-identical to one large call. `execute()` returns a zero-copy
**view** into an internal buffer — `process()` it (or `.copy()` it) before the
next `execute()`:

```python
rc = RateConverter(0.5)
iq_stream = np.array_split(x, 4)   # CF32 blocks, any length
process = np.abs                   # a real downstream consumer

for block in iq_stream:        # CF32 blocks, any length
    y = rc.execute(block)
    process(y)                 # consume now; the next execute() reuses y's buffer
```

!!! warning "View lifetime"

    The returned array is valid only until you next touch the converter.
    **Copy it to retain it.** The next `execute()` reuses the buffer in place;
    `reset()`, assigning `.rate`, or a block larger than any seen so far
    *reallocates* it (a previously-returned array then dangles). The fixed-block
    consume-then-next loop above needs no copy. This is the convention for every
    `variable_output` execute in doppler (`Resampler`, `FIR`, the DDC chain, …).

### Serializable state — elastic resume

A `RateConverter` can hand its entire running state to a fresh instance and
resume **bit-for-bit** — the basis for checkpointing, migrating a stream between
processes, or scaling a pipeline across pods. `get_state()` returns a `bytes`
blob; `set_state()` restores it into an identically-built converter:

```python
>>> import numpy as np
>>> from doppler.resample import RateConverter
>>> rng = np.random.default_rng(0)
>>> x = (rng.standard_normal(6000)
...      + 1j * rng.standard_normal(6000)).astype(np.complex64)

>>> # Worker A processes the first half, checkpoints its state, then exits.
>>> a = RateConverter(0.5)
>>> head = np.array(a.execute(x[:2600]))
>>> blob = a.get_state()          # bytes — persist or ship to another worker
>>> len(blob) == a.state_bytes()
True
>>> del a

>>> # Worker B restores the exact mid-stream state and resumes.
>>> b = RateConverter(0.5)        # same rate ⇒ same cascade
>>> b.set_state(blob)
>>> tail = np.array(b.execute(x[2600:]))

>>> # The hand-off is seamless: identical to one uninterrupted run.
>>> ref = RateConverter(0.5)
>>> reference = np.array(ref.execute(x))
>>> np.array_equal(np.concatenate([head, tail]), reference)
True

```

The blob carries a self-describing envelope, so a truncated, corrupted, or
wrong-configuration blob is **rejected** (`ValueError`) rather than silently
reinterpreted — `set_state` either restores exactly or leaves the converter
untouched:

```python
>>> for bad in [blob[:-1],                          # truncated
...             RateConverter(0.25).get_state()]:   # different rate ⇒ different size
...     try:
...         RateConverter(0.5).set_state(bad)
...     except ValueError:
...         print("rejected")
rejected
rejected

```

`get_state`/`set_state`/`state_bytes` are uniform across every serializable
doppler type — see the [State Serialization design](../design/state-serialization.md).

### Functional wrapper

`rate_convert()` creates a `RateConverter` on the first call and returns it
so it can be passed back to maintain state:

```python
from doppler.resample import rate_convert

y1, rc = rate_convert(x, 0.5)
y2, rc = rate_convert(x, 0.5, rc=rc)   # same converter, state preserved
```

### CIC droop compensation

Pass `compensate=1` to append a passband-droop compensating FIR after any
CIC stage. The FIR is designed with `ciccompmf(N=4, R=R, M=7)`:

```python
rc = RateConverter(0.125, compensate=1)
# cascade: CIC(8) → FIR-comp(7 taps)
print(rc.stages)   # ['CIC(8)+FIR']
```

______________________________________________________________________

## `Resampler` — general polyphase

Built-in Kaiser bank (60 dB rejection, 0.4/0.6 pass/stop). Works for
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
```

### Rate-controlled resampling (FM/Doppler correction)

Per-sample rate deviation via `execute_ctrl`:

```python
doppler_correction = np.linspace(-1.0, 1.0, 4096)  # per-sample deviation
ctrl = np.zeros(4096, dtype=np.complex64)  # deviation in norm_freq units
ctrl.real = 1e-4 * doppler_correction
y = r.execute_ctrl(x, ctrl)
```

______________________________________________________________________

## `HalfbandDecimator` — fixed 2:1 decimation

Symmetric FIR halfband filter; every other output sample is the identity
(zero multiply) which halves the compute cost vs. a general FIR.
Use as the first stage in a multi-stage DDC chain.

```python
from doppler.resample import HalfbandDecimator, kaiser_beta, kaiser_num_taps
import numpy as np

# Design a Kaiser halfband prototype (the caller supplies the FIR taps).
ntaps = kaiser_num_taps(2, 60.0, 0.4, 0.6) | 1     # odd length (19 taps)
n = np.arange(ntaps) - (ntaps - 1) // 2
h = np.sinc(n / 2.0) * np.kaiser(ntaps, kaiser_beta(60.0))
h = (h / h.sum()).astype(np.float32)               # unit DC gain

decim = HalfbandDecimator(h=h)       # caller-supplied Kaiser prototype
x = np.random.randn(4096).astype(np.complex64)
y = decim.execute(x)                 # len(y) = 2048
```

Phase-continuous across blocks:

```python
next_stage = np.abs                  # a real downstream consumer

for block in iq_stream:              # CF32 arrays, any length
    y = decim.execute(block)
    next_stage(y)
```

______________________________________________________________________

______________________________________________________________________

## `CIC` — cascaded integrator-comb decimator

Fixed-rate integer decimation by a power-of-two factor R. Fixed at N=4
stages, M=1. Runs directly on the input stream at full rate — no
multipliers, just integrators and combs. Pair with `ciccompmf` to correct
passband droop.

```python
from doppler.resample import CIC, ciccompmf
import numpy as np

cic = CIC(16)                   # R=16, N=4 (fixed), M=1 (fixed)
x = np.random.randn(4096).astype(np.complex64)
y = cic.decimate(x)             # len(y) = 256

# Design a 7-tap droop compensator (runs at output rate)
h = ciccompmf(N=4, R=16, M=7)  # NDArray[float64], length 7
```

______________________________________________________________________

## `ciccompmf` — CIC droop-compensator design

Closed-form maximally-flat FIR compensator (Molnar & Vucic, IEEE TCAS-II
58(12):926–930, 2011). Returns a symmetric FIR kernel that corrects CIC
passband droop; apply it at the decimated output rate.

```python
from doppler.resample import ciccompmf

h = ciccompmf(N=4, R=16, M=7)
# h is NDArray[float64] of length M=7, DC gain ≈ 1.0
```

Valid M: odd 1–19, even 2–18. Out-of-range M returns all-zeros.

______________________________________________________________________

## Farrow — fractional-delay interpolator

`Farrow` is a lightweight **selectable-order** fractional-delay interpolator
(`linear` / `parabolic` / `cubic`) — the lean alternative to a full polyphase
resampler when all you need is a fractional tap, e.g. the interpolator inside a
symbol-timing loop. All three orders share one 4-tap delay line and a fixed
2-sample group delay (so a driving loop is order-agnostic) and are symmetric
about the interpolation point (linear-phase → no delay bias). The fractional
offset `µ ∈ [0,1)` is meant to come from an integer timing NCO, so timing stays
exact while only the interpolation is floating point.

See the [Farrow gallery page](../gallery/farrow.md) for the response of each
order.

```python
from doppler.resample import Farrow

f = Farrow(order="cubic")   # "linear" | "parabolic" | "cubic"
y = f.delay(x, 0.3)         # constant fractional delay (mu) of a cf32 block
```

______________________________________________________________________

## Kaiser filter-design helpers

Two closed-form helpers expose the Kaiser window design the polyphase
`Resampler` uses internally, for callers rolling their own FIR spec.
`kaiser_beta(atten)` maps a stopband attenuation (dB) to the Kaiser `beta`
shape parameter (the FIR-stopband formula `0.1102·(A−8.7)`, distinct from the
window-sidelobe formula in `spectral`); `kaiser_num_taps(num_phases, atten, pb, sb)` returns the tap count meeting an attenuation over a pass/stop band edge.

```python
from doppler.resample import kaiser_beta, kaiser_num_taps

beta = kaiser_beta(60.0)                       # 60 dB stopband → beta ≈ 5.65
ntaps = kaiser_num_taps(4096, 60.0, 0.4, 0.6)  # taps for a 0.4/0.6 transition
```

______________________________________________________________________

::: doppler.resample
options:
members:
\- RateConverter
\- rate_convert
\- Resampler
\- HalfbandDecimator
\- HalfbandDecimatorDp
\- HalfbandDecimatorR2C
\- CIC
\- ciccompmf
\- Farrow
\- kaiser_beta
\- kaiser_num_taps
