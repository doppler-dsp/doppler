# Architecture

Doppler is a stack of four layers. Each layer is independently
useful; together they take you from raw DSP primitives to a
running multi-process signal pipeline in a handful of commands.

<div style="max-width: 540px; margin: 0 auto;">

```mermaid
block-beta
  columns 1
  apps["Apps & Tools — specan, your own sinks & UIs"]
  cli["Pipeline CLI — doppler compose (YAML + Dopplerfile)"]
  transport["Transport — ZMQ streaming (PUSH/PULL, PUB/SUB)"]
  block:dsp:1
    columns 2
    core["DSP Library — C99 core (NCO, FIR, FFT, DDC, Resampler, Buffer)"]:2
    python["Python (thin ctypes)"]
    rust["Rust FFI (safe wrap)"]
  end
```

</div>

---

## Layer 1 — DSP Library (C99 core)

The entire algorithm library lives in one portable C99 library.
NCO, FIR, FFT, DDC, polyphase resampler, ring buffers — each
implemented once, tested once, and callable from any language
through the `dp_*` C ABI.

**Language bindings are thin wrappers over this ABI.** The Python
`doppler` package and the Rust FFI crate both call the same C
functions. There is no Python reimplementation of the NCO, no Rust
port of the FIR engine — just glue: type conversion, error
translation, memory lifetime.

See the [Overview](overview.md) for the full API.

---

## Layer 2 — Transport (ZMQ streaming)

`dp/stream.h` adds a ZMQ-backed wire protocol on top of the DSP
library. It defines one header struct (`dp_header_t`), one magic
value (`SIGS`), and three messaging patterns:

| Pattern | Use |
|---------|-----|
| **PUSH/PULL** | Unidirectional pipeline — source → block → sink |
| **PUB/SUB** | Fan-out — one source, many subscribers |
| **REQ/REP** | Request/response — configuration, queries |

Every block speaks the same framing format. A C transmitter can
push to a Python subscriber and vice versa. The transport is
optional — if you only need the DSP primitives, skip it entirely.

See [API: Streaming](api/python-streaming.md).

---

## Layer 3 — Pipeline CLI (`doppler compose`)

`doppler compose` is a process orchestrator that wires blocks into
pipelines using the streaming transport. You describe a chain in
a YAML file (or generate one with `compose init`); `compose up`
assigns ports, spawns each block as an independent OS process, and
tracks their state.

```sh
doppler compose init tone fir specan --name demo
doppler compose up demo
doppler compose ps
doppler compose down demo
```

Custom blocks are defined in a **Dopplerfile** — a small YAML file
that names an entry-point function and its dependencies. No C
required; any Python (or compiled binary) that reads from a PULL
socket and writes to a PUSH socket qualifies as a block.

See [CLI & Pipelines](cli/index.md) and [Dopplerfile](cli/dopplerfile.md).

---

## Layer 4 — Apps & Tools

**Spectrum analyzer (`doppler-specan`)** is a first-class app built
on the streaming layer. It runs as a pipeline sink, reads IQ frames
over a PULL socket, computes FFT magnitude, and serves a live web
UI. Because it speaks the same wire format as every other block, it
snaps onto any compose pipeline as a final stage — or runs
standalone against any `dp_pub` source.

```sh
# As a compose sink
doppler compose init tone specan --name view

# Or wire it into your own pipeline
doppler specan --port 5600
```

See [Spectrum Analyzer](specan/index.md).

---

## A complete flow

```mermaid
flowchart LR
    subgraph compose ["doppler compose up demo"]
        CFG["demo.yml"]
    end

    TONE["tone\nsource"]
    FIR["fir\nDSP block"]
    SPECAN["specan\nsink"]

    compose -- "spawns + tracks" --> TONE & FIR & SPECAN
    TONE -- "PUSH :5600" --> FIR
    FIR  -- "PUSH :5601" --> SPECAN
```

The compose runner is the only process that knows the full
topology. Each block only knows its upstream PULL address and
downstream PUSH address — the C library does the actual signal
processing, and ZMQ moves the frames between them.
