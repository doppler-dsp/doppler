---
description: "Tune, filter and decimate to baseband: which down-converter to use, retuning and phase continuity, matched mode, the real-input Ddcr, streaming without allocation, and threading."
---

# Down-Conversion

Digital Down-Converter — shifts a carrier to baseband and decimates in
one call, backed by `dp_ddc_state_t` and `dp_ddcr_state_t`.

Source:
[`src/doppler/ddc/__init__.py`](https://github.com/doppler-dsp/doppler/blob/v0.83.0/src/doppler/ddc/__init__.py)

## Which class to use

Two types, one per input dtype — and each has a **matched flavor** built by a
different constructor over the same C state:

| Class         | Input        | Terminal stage      | Use when                           |
| ------------- | ------------ | ------------------- | ---------------------------------- |
| `DDC`         | CF32 IQ      | Kaiser anti-alias   | complex ADC, already at fs         |
| `MatchedDDC`  | CF32 IQ      | matched-filter bank | recovering symbols from IQ         |
| `Ddcr`        | float32 real | Kaiser anti-alias   | real ADC, direct-sampling SDR      |
| `MatchedDdcr` | float32 real | matched-filter bank | recovering symbols from a real ADC |

All four produce CF32 IQ at the decimated output rate and share the same
processing methods (`Ddcr`/`MatchedDdcr` also have `close()`), and take an optional caller-provided output buffer (`y = ddc.execute(x, out)`)
when allocation and buffer reuse need to be explicit.

## `DDC` — complex input

Signal chain: LO mix → RateConverter (the cheapest CIC + halfband + polyphase
cascade for the rate, chosen at create time).
Built-in Kaiser bank (60 dB rejection) — no filter design required.

**Frequency convention:** `norm_freq = -f_carrier` shifts a carrier at
`f_carrier` (normalised to fs) down to DC.

```python
from doppler.ddc import DDC
import numpy as np

# Tune to a tone at +0.1·fs, decimate 4×
ddc = DDC(norm_freq=-0.1, rate=0.25)

x = np.random.randn(4096).astype(np.complex64)
y = ddc.execute(x)          # CF32 output, len(y) ≈ 4096 * 0.25 = 1024

print(f"in={len(x)}  out={len(y)}")
```

### Retune without reset

```python
ddc.norm_freq = -0.2        # LO retuned; resampler history preserved
next_block = np.random.randn(4096).astype(np.complex64)
y = ddc.execute(next_block)
```

### Phase-continuous across blocks

```python
ddc = DDC(-0.1, 0.25)
iq_stream = [np.random.randn(4096).astype(np.complex64) for _ in range(3)]

def process(chunk):         # your per-block consumer
    return chunk

for block in iq_stream:     # generator of CF32 arrays
    out = ddc.execute(block)
    process(out)
```

### Matched mode

`MatchedDDC` is the same object as `DDC` built by a different constructor: the
cascade's **terminal stage carries the matched filter** instead of the default
Kaiser anti-alias bank, so the same dot products that mix and decimate also
matched-filter, and that stage's polyphase arm becomes the fractional timing
delay a loop steers. It is a straight passthrough to
[`RateConverter`][doppler.resample.RateConverter] — the DDC adds the mix in front of it.
`MatchedDdcr` is the identical flavor of the real-input chain.

```python
from doppler.ddc import MatchedDDC
import numpy as np

# 16 samples/symbol in, 2 out; carrier at +0.09375·fs wiped off on the way.
rx = MatchedDDC(norm_freq=-0.09375, rate=2 / 16, pulse="rrc", beta=0.35,
                span=8, pulse_sps=2.0)

x = (0.25 * np.random.randn(4096)).astype(np.complex64)
symbols = rx.execute(x)          # matched-filtered, 2 samples/symbol
print(len(symbols), rx.clipped)  # clipped: has any CIC stage clipped?
```

`pulse_sps` is the pulse's period in **output** samples, so a caller wanting
`m` samples per symbol at `sps` input samples per symbol asks for
`rate = m/sps` and `pulse_sps = m`. CIC droop compensation is unconditional on
this path — the fold costs six taps per arm and is worth 28 dB.

### Two control ports

A matched DDC is steerable on two accumulators, and they are duals of each
other:

| port        | steers                           | units                               | loop it closes |
| ----------- | -------------------------------- | ----------------------------------- | -------------- |
| `freq_ctrl` | the LO's phase accumulator       | cycles/sample at the **input** rate | carrier        |
| `rate_ctrl` | the terminal stage's accumulator | output periods per terminal input   | timing         |

Neither deviation is persisted — `norm_freq` and `rate` never move — so a
tracking loop passes its full filter output on every call and the DDC holds no
loop state of its own. `execute_ctrl` applies both across a block (open loop: a
fixed Doppler offset, a rate trim); `execute_ctrl_push` applies them one input
at a time, which is the only form a closed loop can use, since each correction
is computed *from* the outputs already emitted.

```python
# Carrier recovery: a tone 0.01 cycles/sample off where the LO is tuned.
f0, tuned, rate, mu = 0.05, -0.04, 0.25, 0.01
d = MatchedDDC(tuned, rate, pulse="rrc")
x = (0.25 * np.exp(2j * np.pi * f0 * np.arange(8192))).astype(np.complex64)

freq_ctrl, prev = 0.0, None
for i, v in enumerate(x):
    for y in d.execute_ctrl_push(complex(v), 0.0, freq_ctrl):
        if prev is not None and i > 1024:       # let the cascade prime
            e = float(np.angle(y * np.conj(prev)) / (2 * np.pi))
            freq_ctrl -= mu * e * rate          # e is per OUTPUT sample
        prev = y

assert abs(f0 + tuned + freq_ctrl) < 1e-6       # parked on the mistune
assert d.norm_freq == tuned                     # the centre never moved
```

The loop gain is small for a structural reason: the loop closes **around** the
matched filter, so its dead time is that filter's group delay. Measured on this
configuration, `mu` of 0.01–0.02 converges, 0.05 is marginal and 0.1 diverges —
the same reason a real receiver's carrier-loop bandwidth is a small fraction of
the symbol rate.

The timing port is the other half: wrap a detector and a loop filter around
`rate_ctrl` and you have [`track.RateSync`][doppler.track.RateSync], which does exactly
that over a `RateConverter`.

## `Ddcr` — real input (Architecture D2)

Signal chain: halfband R2C (2:1, embedded −fs/4 shift, zero extra
multiplications) → LO mix at fs/2 → polyphase resample.

Choose it because your input is real, not for speed. The halfband's embedded
fs/4 mix costs no multiplications, but the front end alone measures 1.04–1.40×
against `DDC` on the same stream promoted to complex (0.74–1.13× once the
promote is charged to `DDC`); the half rate pays in a whole receiver, where it
halves the rate ahead of the matched filter (1.13–1.69× for `MpskReceiverR`,
see `ddcr_core.h`).

`Ddcr` wraps `dp_ddcr_state_t`. `execute()` returns its own array, or fills a
**caller-provided writable `complex64` buffer** and returns the trimmed view
`out[:n_out]` when one is passed — so allocation and buffer reuse can be made
explicit for streaming and sharded-worker designs.

**Frequency convention:** `norm_freq = -(2*f_carrier + 0.5)`
The −0.5 cancels the halfband's embedded −fs/4 shift.

```python
from doppler.ddc import Ddcr
import numpy as np

# Tune to a tone at f_carrier=0.1·fs; real ADC, decimate 4×
#   norm_freq at intermediate rate: -(2 * 0.1 + 0.5) = -0.7
ddcr = Ddcr(norm_freq=-0.7, rate=0.25)

x = np.random.randn(4096).astype(np.float32)   # real ADC samples
out = np.empty(len(x), dtype=np.complex64)     # caller buffer (reuse across calls)
y = ddcr.execute(x, out)    # CF32 view out[:n_out], len(y) = 4096 * 0.25 = 1024 (rate is fs_out / fs_in)
```

## `Ddcr` usage patterns

Supplying the output buffer makes allocation strategy and buffer reuse
entirely explicit — ideal for streaming and sharded-worker designs.

!!! warning "Match the buffer's dtype"

    An `out=` buffer of the wrong dtype is refused with `TypeError` rather
    than cast into a temporary. Always allocate `complex64`.

### Buffer sizing

A decimating `Ddcr` never produces more output samples than it consumes. An
output buffer of `len(x)` elements is always sufficient; `execute` returns the
trimmed zero-copy view `out[:n_out]`:

```python
out = np.empty(len(x), dtype=np.complex64)
y = ddcr.execute(x, out)            # len(y) <= len(x), zero-copy view of out
```

### Streaming loop — no per-call allocation

```python
import numpy as np
from doppler.ddc import Ddcr

ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
out = np.empty(4096, dtype=np.complex64)   # allocate once, reuse

def real_adc_stream():                     # your ADC source
    for _ in range(3):
        yield np.random.randn(4096).astype(np.float32)

for x in real_adc_stream():                # x: float32, len 4096
    y = ddcr.execute(x, out)               # y is out[:n_out], zero-copy
    process(y)
```

### Retune mid-stream

```python
new_carrier = 0.15
ddcr.norm_freq = -(2 * new_carrier + 0.5)  # writable property; phase-continuous
y = ddcr.execute(x, out)
```

`rate` is read-only (fixed at construction); `norm_freq` is live-writable.

### Multiple independent streams

```python
num_channels, N = 4, 4096
x = [np.random.randn(N).astype(np.float32) for _ in range(num_channels)]
y = [None] * num_channels
chans = [Ddcr(-0.7, 0.25) for _ in range(num_channels)]
bufs  = [np.empty(N, dtype=np.complex64) for _ in range(num_channels)]

for ch, (d, buf) in enumerate(zip(chans, bufs)):
    y[ch] = d.execute(x[ch], buf).copy()   # copy if it must outlive the next call
```

### Throughput

`Ddcr` and `DDC` call the same C kernel; the per-call Python overhead is under
1 µs on 65 536-sample blocks.

### Threading — the GIL is released across the kernel

`Ddcr.execute` (like `DDC.execute`) runs the pure-C kernel with the **GIL
released** (`Py_BEGIN_ALLOW_THREADS`; numpy accessors hoisted out first). So a
**thread-per-shard** worker — each thread owning its own `Ddcr` and output
buffer — scales across cores instead of serialising on the GIL:

```python
import threading

N = 4096
shards = [[np.random.randn(N).astype(np.float32)] for _ in range(4)]

def worker(ddcr, blocks, out):
    for x in blocks:                 # this thread's own handle + buffer
        process(ddcr.execute(x, out))

threads = [
    threading.Thread(target=worker, args=(Ddcr(-0.7, 0.25),
                                          shard, np.empty(N, np.complex64)))
    for shard in shards
]
for t in threads: t.start()
for t in threads: t.join()
```

Measured ~5–8× across 8–12 cores (then memory-bandwidth bound). **Contract:**
one `Ddcr` per stream — never share a handle across threads concurrently (no
internal lock). This is the basis of the sharded-microservice model; see
the [Ddcr gallery walkthrough](../gallery/ddc-fn.md).

### Lifecycle and memory safety

`Ddcr` owns its C state: `close()` (or a `with` block) releases the C state
deterministically; otherwise the destructor frees it.

| Scenario                  | Safe?                                                   |
| ------------------------- | ------------------------------------------------------- |
| GC without `close()`      | Yes — destructor frees state                            |
| `close()` then GC         | Yes — destructor skips already-freed state              |
| Live view after `close()` | Yes — view lives in caller's `out` buffer, not in state |
| Second call to `close()`  | No-op (idempotent)                                      |
| Any call after `close()`  | Raises `RuntimeError`                                   |

```python
x = np.random.randn(4096).astype(np.float32)
out = np.empty(4096, dtype=np.complex64)
with Ddcr(-0.7, 0.25) as ddcr:      # state released on exit
    y = ddcr.execute(x, out)
    process(y)
```
