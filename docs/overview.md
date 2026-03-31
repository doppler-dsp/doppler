# doppler — Overview

doppler is a lean C99 signal processing library. The full DSP stack —
NCO, FIR, FFT, SIMD arithmetic, ring buffers, and ZMQ streaming — lives
in one portable core with a clean C ABI. Python and Rust bindings call
straight through to C with no reimplementation. ZMQ streaming is optional:
include it when you need to move signals across processes or machines.

---

## Signal Processing Core

### NCO

Header: [c/include/dp/nco.h](../c/include/dp/nco.h)

32-bit overflowing phase accumulator with a 2¹⁶-entry sine LUT (~96 dBc
SFDR). `dp_nco_execute_cf32` uses an AVX-512 16-wide gather path when
available, falling back to scalar automatically. An FM control port
accepts a per-sample normalized frequency deviation.

```c
#include <dp/nco.h>

// Create an NCO at normalized frequency f_n = 0.25 (quarter rate)
dp_nco_t *nco = dp_nco_create(0.25f);

// Generate IQ samples
dp_cf32_t out[1024];
dp_nco_execute_cf32(nco, out, 1024);

// Raw uint32 phase (no LUT, fastest path)
uint32_t phase[1024];
dp_nco_execute_u32(nco, phase, 1024);

// Overflow / carry bit (wraps → 1 on each full cycle)
uint8_t carry[1024];
dp_nco_execute_u32_ovf(nco, phase, carry, 1024);

// FM: add per-sample frequency deviation from a ctrl array
float ctrl[1024];  /* normalized deviation per sample */
dp_nco_execute_cf32_ctrl(nco, ctrl, out, 1024);

dp_nco_reset(nco);             // return phase to zero
dp_nco_set_freq(nco, 0.1f);    // change frequency (phase preserved)
dp_nco_destroy(nco);
```

**Python:**

```python
from doppler import Nco
import numpy as np

with Nco(0.25) as nco:
    iq   = nco.execute_cf32(1024)     # complex64 array
    ph   = nco.execute_u32(1024)      # uint32 phase
    ph, carry = nco.execute_u32_ovf(1024)

    ctrl = np.zeros(1024, dtype=np.float32)
    iq   = nco.execute_cf32_ctrl(ctrl)
```

### FIR filter

Header: [c/include/dp/fir.h](../c/include/dp/fir.h)

Complex-tap FIR with AVX-512 acceleration. Accepts CI8, CI16, CI32, and
CF32 input types; always outputs CF32.

```c
#include <dp/fir.h>

// Build a low-pass filter (real taps stored as CF32 with q=0)
dp_cf32_t taps[19];
/* ... fill taps with sinc × window ... */
dp_fir_t *fir = dp_fir_create(taps, 19);

dp_cf32_t in[1024], out[1024];
dp_fir_execute_cf32(fir, in, out, 1024);

dp_ci16_t in16[1024];
dp_fir_execute_ci16(fir, in16, out, 1024);   // 16-bit IQ → CF32 out

dp_fir_reset(fir);     // flush delay line
dp_fir_destroy(fir);
```

### Engine lifecycle

Header: [c/include/doppler.h](../c/include/doppler.h)

```c
#include <doppler.h>

dp_init();      // Initialize global state (FFT backend, SIMD, etc.)
// ... do work ...
dp_cleanup();   // Clean shutdown, free FFT plans
```

### FFT

Header: [c/include/dp/fft.h](../c/include/dp/fft.h)

1D and 2D FFTs. Set up once, execute many times. The FFT backend is
selectable — FFTW (default, high-performance) or pocketfft (MIT-only).
See [Build Guide](build.md) for details.

```c
#include <dp/fft.h>
#include <complex.h>

// Setup (once per shape)
size_t shape[] = {1024};
dp_fft_global_setup(
    shape,       // array dimensions
    1,           // ndim
    -1,          // sign: -1 = forward, +1 = inverse
    1,           // nthreads
    "estimate",  // planner: "estimate", "measure", "patient" (FFTW only)
    NULL         // wisdom path (FFTW only, optional)
);

// Execute
double complex in[1024], out[1024];
dp_fft1d_execute(in, out);          // out-of-place
dp_fft1d_execute_inplace(in);       // in-place

// 2D
size_t shape2d[] = {64, 64};
dp_fft_global_setup(shape2d, 2, -1, 1, "estimate", NULL);
dp_fft2d_execute(in2d, out2d);
dp_fft2d_execute_inplace(in2d);
```

### Circular buffers

Header: [c/include/dp/buffer.h](../c/include/dp/buffer.h)

Double-mapped lock-free SPSC ring buffers. The virtual-memory mapping
makes every contiguous read possible without a copy — the second mapping
wraps the buffer behind the first, so reads never straddle the edge.

```c
#include <dp/buffer.h>

dp_f32 *buf = dp_f32_create(1 << 20);   // 1M float32 elements

/* producer */
dp_f32_write(buf, samples, N);

/* consumer — wait returns a contiguous view, no copy needed */
float *view = dp_f32_wait(buf, N);
/* ... process view[0..N-1] ... */
dp_f32_consume(buf, N);

dp_f32_destroy(buf);
```

Types: `dp_f32` (float32 elements), `dp_f64` (float64 elements),
`dp_i16` (int16 elements).  See [Data Types](api/datatypes.md) for
how buffer element types relate to complex sample types.

### Utility arithmetic

Header: [c/include/dp/util.h](../c/include/dp/util.h)

SSE2/NEON/scalar complex multiply (`dp_c16_mul`).

```c
#include <dp/util.h>
#include <complex.h>

double complex a = 1.0 + 2.0*I;
double complex b = 3.0 + 4.0*I;
double complex c = dp_c16_mul(a, b);  // (1+2i)(3+4i) = -5+10i
```

---

## Streaming Transport

ZMQ-based streaming moves signals between nodes — processes, containers, or
machines. It is not required for DSP; use it when you need data transport.

Header: [c/include/dp/stream.h](../c/include/dp/stream.h)

### Sample types

| Type | Storage | Size |
| ---- | ------- | ---- |
| `DP_CI32` | `int32_t i, q` | 8 bytes/sample |
| `DP_CF64` | `double i, q` | 16 bytes/sample |
| `DP_CF128` | `long double i, q` | 32 bytes/sample |

### Publisher / subscriber (PUB/SUB)

Fan-out streaming: one transmitter, many receivers.

```c
#include <dp/stream.h>

// Publish
dp_pub *tx = dp_pub_create("tcp://*:5555", DP_CF64);
dp_pub_send_cf64(tx, samples, count, sample_rate, center_freq);
dp_pub_destroy(tx);

// Subscribe (zero-copy)
dp_sub *rx = dp_sub_create("tcp://localhost:5555");
dp_msg_t *msg;
dp_header_t hdr;
dp_sub_recv(rx, &msg, &hdr);
dp_cf64_t *data = (dp_cf64_t *)dp_msg_data(msg);
size_t n = dp_msg_num_samples(msg);
// use data[0..n-1] ...
dp_msg_free(msg);
dp_sub_destroy(rx);
```

### Pipeline (PUSH/PULL)

Load-balanced pipeline: work distributed across multiple consumers.

```c
// Producer (binds)
dp_push *tx = dp_push_create("ipc:///tmp/pipe.ipc", DP_CF64);
dp_push_send_cf64(tx, samples, count, sample_rate, center_freq);
dp_push_destroy(tx);

// Consumer (connects)
dp_pull *rx = dp_pull_create("ipc:///tmp/pipe.ipc");
dp_msg_t *msg;
dp_header_t hdr;
dp_pull_recv(rx, &msg, &hdr);
dp_cf64_t *data = (dp_cf64_t *)dp_msg_data(msg);
// use data ...
dp_msg_free(msg);
dp_pull_destroy(rx);
```

### Message header

Each message carries: sequence number, nanosecond timestamp, sample rate,
center frequency, and sample type — enough to reconstruct timing and frequency
context at the receiver with no side-channel state.

### Request / reply

A synchronous REQ/REP pattern is available for control messages and metadata
queries alongside the PUB/SUB data plane.

---

## Python

### FFT (Python C extension)

```python
from doppler.fft import fft, setup, execute1d, execute2d, execute1d_inplace
import numpy as np

# One-shot (setup + execute)
spectrum = fft(x)

# Reuse plan for repeated transforms
setup(x.shape, nthreads=4, planner="measure")
for batch in stream:
    out = execute1d(batch)       # out-of-place
    execute1d_inplace(batch)     # or in-place

# 2D
setup((64, 64))
out = execute2d(x2d)
```

Input arrays must be `complex128` (double-precision complex).

### Streaming (Python)

Full Python bindings are provided by the `doppler.stream` subpackage.

```python
from doppler import Publisher, Subscriber, Push, Pull, CF64
import numpy as np

samples = np.array([1+2j, 3+4j], dtype=np.complex128)

# PUB/SUB
with Publisher("tcp://*:5555", CF64) as pub:
    pub.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Subscriber("tcp://localhost:5555") as sub:
    data, hdr = sub.recv(timeout_ms=500)

# PUSH/PULL
with Push("tcp://*:5556", CF64) as push:
    push.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Pull("tcp://localhost:5556") as pull:
    data, hdr = pull.recv(timeout_ms=500)
```

### Circular buffers (Python)

Double-mapped ring buffers for zero-copy, lock-free producer/consumer pipelines.

```python
from doppler import F64Buffer
import numpy as np

buf = F64Buffer(256)           # 256 complex128 samples

buf.write(np.ones(256, dtype=np.complex128))   # producer: non-blocking
view = buf.wait(256)                           # consumer: blocks until ready
buf.consume(256)                               # release samples
```

See [Examples](examples.md) for all buffer types and a threaded example.

---

## Language support

doppler exposes a clean C ABI. Any language with C FFI can call it
directly.

| Language | Status |
| -------- | ------ |
| C | Native — full API |
| Python | NCO, FFT, FIR, resample, streaming, buffers, accumulator, delay (`doppler` package) |
| Rust | FFI bindings (`ffi/rust/`) |
| C++ | Works via `extern "C"` headers |

---

## Architecture

```text
┌─────────────────────────────────────┐
│  Your Application                   │
│  Python / Rust / C++ / ...          │
└──────────────┬──────────────────────┘
               │ C ABI / ctypes / FFI
┌──────────────▼──────────────────────┐
│  doppler (C library)                │
│  • NCO  (AVX-512 batch IQ / phase)  │
│  • FIR  (AVX-512, CI8/CI16/CF32)    │
│  • FFT  (FFTW or pocketfft)         │
│  • Util (SSE2/NEON complex mul)      │
│  • Ring buffers (lock-free SPSC)    │
│  • Streaming (ZMQ, optional)        │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  System libraries                   │
│  • FFTW3 or pocketfft (FFT)         │
│  • libzmq (streaming, optional)     │
│  • libc, pthreads                   │
└─────────────────────────────────────┘
```

---

## See also

- [README](../README.md) — Project intro and quick start
- [Quick Start](quickstart.md) — Get running in minutes
- [Build Guide](build.md) — Build options and platform notes
- [Examples](examples.md) — C and Python code examples
