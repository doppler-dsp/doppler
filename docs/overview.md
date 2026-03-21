# doppler — Overview

doppler is a C library for signal processing at native speed. The core
is DSP: FFT, SIMD-accelerated arithmetic, and a clean C ABI designed for
calling from any language. ZMQ-based streaming is available as an optional
transport layer when you need to move signals between processes or machines.

---

## Signal Processing Core

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

### SIMD arithmetic

Header: [c/include/dp/simd.h](../c/include/dp/simd.h)

AVX2-accelerated complex multiply.

```c
#include <dp/simd.h>
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

Full Python bindings are provided by the `dp_stream` C extension module.

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
| Python | FFT (C ext), streaming + circular buffers (`doppler` package) |
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
│  doppler (C library)          │
│  • FFT (FFTW or pocketfft + SIMD)   │
│  • SIMD arithmetic (AVX2)           │
│  • Signal streaming (ZMQ, optional) │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  System libraries                   │
│  • FFTW3 or pocketfft (FFT)         │
│  • libzmq (messaging, optional)     │
│  • libc, pthreads                   │
└─────────────────────────────────────┘
```

---

## See also

- [README](../README.md) — Project intro and quick start
- [Quick Start](quickstart.md) — Get running in minutes
- [Build Guide](build.md) — Build options and platform notes
- [Examples](examples.md) — C and Python code examples
