# SPECAN Design

**Super fast and intuitive, out-of-the-box ready to rock!**

---

## User Experience

- `uvx doppler-specan` — launches terminal display
- `uvx doppler-specan --web` — launches browser UI
- Simple, minimal, powerful
- Fully configurable via `doppler-specan.yml` and/or CLI flags (CLI overrides yml)

### CLI flags

| Flag | Default | Description |
|------|---------|-------------|
| `--level` | `0` dBm | Reference level — top of display |
| `--center` | `0` Hz | Center frequency |
| `--span` | auto | Alias-free bandwidth; set by resampler = `Fs_out / 1.28` |
| `--rbw` | `span / 401` | Resolution bandwidth (ENBW of Kaiser window) |
| `--source` | `demo` | `demo`, `file <PATH>`, `socket <ADDRESS>` |
| `--web` | off | Launch browser UI instead of terminal display |
| `--no-browser` | off | Start web server without opening a browser |
| `--port` | `8765` | Web server port |

### `doppler-specan.yml`

Single source of truth for persistent configuration. CLI flags override it at
runtime. The web UI's tune/zoom controls update the running state only — they
do not write back to the file unless the user explicitly saves.

```yaml
source: demo          # demo | file | socket
# source: file
# path: /path/to/capture.iq
# source: socket
# address: tcp://localhost:5555

center: 0             # Hz
span: 200e3           # Hz  (auto if omitted)
rbw: 500              # Hz  (auto = span/401 if omitted)
level: 0              # dBm reference level (top of display)

demo:
  tone_freq: 100e3    # Hz offset from center
  tone_power: -20     # dBm
  noise_floor: -90    # dBm
```

---

## Architecture

The engine is the instrument. Both display frontends are consumers of
calibrated dBm frames produced by the DDC engine. They do not reimplement
any signal processing.

```
┌─────────────────────────────────────────────┐
│  Source                                     │
│  socket (ZMQ sub) | file | demo             │
│  → header carries Fs, center, sample type   │
└─────────────────────┬───────────────────────┘
                      │ IQ blocks (cf32)
┌─────────────────────▼───────────────────────┐
│  DDC Engine  (doppler C primitives)         │
│  dp_nco  → mix to DC                        │
│  dp_resamp_dpmfs → decimate to Fs_out       │
│  Kaiser window + dp_fft → power spectrum    │
│  → calibrated dBm frames                   │
└──────────┬──────────────────────┬───────────┘
           │                      │
┌──────────▼──────────┐  ┌───────▼───────────┐
│  Terminal display   │  │  Web UI           │
│  "dumb" viewer      │  │  interactive      │
│  driven by config   │  │  tune & zoom      │
│  blessed / rich     │  │  spur hunting     │
│  headless / SSH     │  │  click to retune  │
│  arrow key retune   │  │  save config      │
└─────────────────────┘  └───────────────────┘
           ↑                      ↑
           └──────────┬───────────┘
              Config layer
              doppler-specan.yml + CLI flags
```

### Two separate modes, one engine

Terminal and web are **separate launch modes**, not a single process. This
keeps the terminal mode dependency-free beyond `rich`/`blessed` — no
fastapi, no uvicorn, no websockets imported. This matters for SSH sessions,
embedded deployments, and scripted automation.

| Mode | Launch | Extra deps | Use case |
|------|--------|------------|----------|
| Terminal | `doppler-specan` | `rich` or `blessed` | SSH, headless, scripted |
| Web | `doppler-specan --web` | `fastapi`, `uvicorn`, `websockets` | Interactive spur hunting |

---

## Signal Processing Chain

### DDC Engine

```
IQ in (cf32, Fs_in)
  → dp_nco: mix (center - source_center) to DC
  → dp_resamp_dpmfs: decimate to Fs_out = span × 1.28
  → Kaiser window (β controls RBW)
  → dp_fft (FFT_SIZE = next_pow2(Fs_out / RBW))
  → magnitude → dBm
  → frame out
```

**SPAN and sample rate:**
`dp_resamp_dpmfs` handles arbitrary decimation ratios — 1.33× and 20056.789×
are equally supported. No cascaded halfband stages are needed as a special
case; the resampler covers it all.

The alias-free output bandwidth of the resampler is approximately `0.8 ×
Fs_out`, so:

```
Fs_out = span / 0.8 = span × 1.25
```

(The 1.28 factor in the design is a conservative margin; exact value TBD
during implementation.)

**RBW and FFT size:**
RBW is the equivalent noise bandwidth (ENBW) of the Kaiser window applied
before the FFT. A β → ENBW lookup table maps the window shape parameter to
the displayed RBW in Hz. FFT_SIZE is rounded to the next power of two:

```
FFT_SIZE = next_pow2(Fs_out / RBW)
```

### Power calibration

Absolute power in dBm referenced to 50 Ω, 1 mW:

```
P_dBm = 10 × log10(|V|² / (2 × 50 × 0.001))
```

An amplitude of 1.0 (full-scale) corresponds to +10 dBm. The `--level`
reference level provides a per-source calibration offset — for SDR hardware
with its own gain/attenuation chain, the user sets `--level` once to align
the display with a known reference.

### Kaiser window

The Kaiser window belongs in the C library (`dp/window.h` or `dp/util.h`),
not Python. The β → ENBW LUT lives there too. The specan Python layer calls
the C function for window generation — it does not reimplement it.

---

## Input Sources

### Socket (ZMQ PUB/SUB)

The doppler streaming protocol (`dp_header_t`) carries `sample_rate`,
`center_freq`, and `sample_type` in every packet header. The DDC engine
auto-configures itself from the first received packet — no `--fs` flag
needed. Subsequent packets that change `sample_rate` trigger a chain
reconfiguration.

```
doppler-specan --source socket tcp://sdr-host:5555
```

### File

Raw interleaved cf32 IQ file. Sample rate and center frequency must be
provided via CLI or yml (no metadata embedded in raw files). Future: support
SigMF files (metadata sidecar).

```
doppler-specan --source file capture.iq --fs 2.048e6 --center 433.92e6
```

### Demo

Generates a calibrated synthetic signal: a complex tone at a declared dBm
power level plus AWGN at a declared noise floor. This is the default source
and the primary way to verify that the power axis is correctly calibrated
before connecting real hardware.

```
doppler-specan  # demo: -20 dBm tone at 100 kHz, -90 dBm noise floor
```

---

## Display Modes

### Terminal

- ASCII spectrum display via `rich` or `blessed`
- Waterfall and/or bar display
- Arrow keys: left/right → retune center; up/down → adjust span
- `r` → reset to config defaults
- `s` → save current state to `doppler-specan.yml`
- Fully scriptable: pipe config via yml for automated frequency search

### Web UI

- Browser-based interactive display (FastAPI + WebSocket)
- Tune & zoom: drag/click to retune center and span
- Spur hunting: click a spectral peak → engine recenters, narrows span
- Live controls: reference level, RBW, source selection
- Save current state to `doppler-specan.yml`

---

## Package Structure

`doppler-specan` is a standalone pip package in `python/specan/` — separate
from `doppler-dsp`. It depends on `doppler-dsp` for all signal processing.
It does **not** live inside the `doppler` namespace.

```
python/specan/
└── doppler_specan/
    ├── __init__.py
    ├── __main__.py      # CLI entry point
    ├── engine.py        # DDC chain + FFT + power calibration
    ├── source.py        # socket / file / demo sources
    ├── config.py        # yml + CLI config loading
    ├── terminal.py      # terminal display (rich/blessed)
    ├── server.py        # FastAPI + WebSocket (web mode only)
    └── static/
        └── index.html   # web UI
```

---

## What Remains in the C Library

Before implementing the specan, the following C primitives are needed:

| Primitive | Header | Status |
|-----------|--------|--------|
| `dp_kaiser_window` | `dp/window.h` | **TODO** |
| β → ENBW LUT / function | `dp/window.h` | **TODO** |
| `dp_nco` | `dp/nco.h` | done |
| `dp_resamp_dpmfs` | `dp/resamp_dpmfs.h` | done |
| `dp_fft` | `dp/fft.h` | done |

Everything else is Python orchestration.
