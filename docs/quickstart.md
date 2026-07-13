# Quick Start

## Get it!

```bash
pip install doppler-dsp
```

The wheel bundles all native dependencies — no system libraries required.
Everything under [Signal processing](#signal-processing) and
[Streaming](#streaming) below runs against this install alone; the
[C transmitter](#c-transmitter-python-subscriber) example binary needs
[Build from source](#build-from-source), called out at each point it
applies. Just want the C **library** itself (headers + `libdoppler.a`/`.so`,
no example binaries)? `jbx get-doppler` grabs a pre-built release tarball
instead — see [C Library](install/c.md#install-from-a-release-tarball).

!!! tip "Optional extras"

    ```bash
    pip install "doppler-dsp[specan]"      # terminal spectrum analyzer
    pip install "doppler-dsp[specan-web]"  # live spectrum analyzer web UI
    pip install "doppler-dsp[cli]"         # compose / Dopplerfile pipeline CLI
    ```

    See [Install → Python](install/python.md) for the full extras table.

!!! tip "Or pull the container image"

    Every release publishes a ready-to-run image with the `cli` and
    `specan-web` extras pre-installed — `doppler`, `doppler-fir`,
    `doppler-source`, `doppler-specan`, and `wfmgen` are all on `PATH`:

    ```bash
    docker pull ghcr.io/doppler-dsp/doppler:latest
    docker run --rm ghcr.io/doppler-dsp/doppler wfmgen --help
    ```

    Built for both `linux/amd64` and `linux/arm64`. See
    [Docker](install/docker.md#published-container) for details.

______________________________________________________________________

## Signal processing

Every object is a thin, stateful wrapper over the C core: construct it once,
then stream blocks through it. Each example below is self-contained —
copy-paste any one of them on its own.

### LO — generate a complex tone

```python
from doppler.source import LO

lo = LO(0.25)          # normalised frequency: 0.25 → Fs/4
iq = lo.steps(8)
print(iq)
# [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j ...]
```

For modulated waveforms, multi-segment scenes, and file/stream output, see
[Waveform Generator (wfmgen)](#waveform-generator-wfmgen) below.

### FFT

```python
from doppler.spectral import FFT
import numpy as np

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
fft = FFT(1024)
X = fft.execute_cf32(x)   # complex64 in → complex64 out (~2× faster than f64)
print(f"FFT: {X.shape[0]} complex64 bins")
```

### FIR filter

```python
from doppler.filter import FIR
from doppler.spectral import kaiser_window, kaiser_beta_for_sidelobe
import numpy as np

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)

n_taps, cutoff = 63, 0.05                    # cutoff: fraction of fs
m = np.arange(n_taps) - (n_taps - 1) / 2
taps = 2 * cutoff * np.sinc(2 * cutoff * m)  # ideal windowed-sinc lowpass
w = np.zeros(n_taps, dtype=np.float32)
kaiser_window(w, kaiser_beta_for_sidelobe(60.0))   # 60 dB sidelobe target
taps = (taps * w).astype(np.float32)

fir = FIR(taps)
y = fir.execute(x)
print(f"filtered {len(y)} samples through a {len(taps)}-tap FIR")
```

### Resample

`RateConverter` picks the cheapest cascade (halfband / CIC / polyphase) for
the rate you ask for — no filter design required:

```python
from doppler.resample import RateConverter
import numpy as np

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
rc = RateConverter(0.5)   # 2:1 decimation → auto-selects a halfband stage
y = rc.execute(x)         # 1024 → 512 samples
print(f"resampled {len(x)} -> {len(y)} samples")
```

______________________________________________________________________

## Streaming

Doppler streams IQ data over NATS. Transmit and receive on the same
machine or across a network — the API is identical. Either side
needs a running `nats-server` (e.g. `nats-server -js`) to connect to.

### Publisher (Python)

<!-- docs-snippet: skip=two-terminal NATS demo, needs a broker; the send/recv round-trip and message fields are gated by doppler/stream/tests/test_stream.py -->

```python
from doppler.stream import Publisher, CF32
import numpy as np

pub = Publisher("nats://127.0.0.1:4222/iq", CF32)   # sample_type must match samples' dtype

samples = np.ones(1024, dtype=np.complex64)
pub.send(samples, sample_rate=1e6, center_freq=2.4e9)
print(f"sent {len(samples)} samples")
```

### Subscriber (Python)

<!-- docs-snippet: skip=blocking recv() with no peer; round-trip covered by doppler/stream/tests/test_stream.py -->

```python
from doppler.stream import Subscriber

sub = Subscriber("nats://127.0.0.1:4222/iq")

samples, hdr = sub.recv()   # (ndarray, header dict) -- not an object with attributes
print(f"Received {len(samples)} samples @ {hdr['sample_rate']/1e6:.1f} MHz")
```

### C transmitter → Python subscriber

Build the C examples once, then mix and match:

```bash
make          # builds ./build/examples/c/transmitter, receiver, etc.

# Terminal 1 (transmitter takes ci32 or cf64 — not cf32)
./build/examples/c/transmitter nats://127.0.0.1:4222/iq cf64

# Terminal 2 (Python)
python - <<'EOF'
from doppler.stream import Subscriber
sub = Subscriber("nats://127.0.0.1:4222/iq")
while True:
    samples, hdr = sub.recv()
    print(f"seq={hdr['sequence']}  samples={len(samples)}")
EOF
```

______________________________________________________________________

## Waveform Generator (wfmgen)

!!! tip "No extra required"

    `wfmgen` ships in the base `pip install doppler-dsp` wheel — no optional
    extra needed.

One engine generates a single waveform, a multi-segment JSON scene, or a live
stream — the CLI and the Python API produce byte-identical output:

```sh
wfmgen --type qpsk --snr 12 --count 100000 -o capture.cf32                    # a single waveform
wfmgen --from-file scenario.json -o scenario.cf32                             # a multi-segment scene
wfmgen --type qpsk --continuous --realtime --output nats://127.0.0.1:4222/iq  # stream to NATS
```

See [Waveform Generator (wfmgen)](guide/wfmgen/index.md) for scenes,
BLUE/SigMF, streaming, and the `Plan` bit-exact sweep cache.

______________________________________________________________________

## Spectrum analyzer

`doppler-specan` opens a live FFT display in your terminal or browser.

!!! note "Requires the `specan` or `specan-web` extra"

    ```bash
    pip install "doppler-dsp[specan]"      # terminal
    pip install "doppler-dsp[specan-web]"  # browser
    ```

**Demo mode** (no hardware needed):

```bash
doppler-specan --source demo
```

**Browser UI:**

```bash
doppler-specan --source demo --web
```

The web UI is served at `http://127.0.0.1:8765` by default.
See [Spectrum Analyzer](specan/index.md) for configuration options.

______________________________________________________________________

## Pipeline CLI

!!! note "Requires the `cli` extra"

    ```bash
    pip install "doppler-dsp[cli]"
    ```

`doppler compose` wires blocks into a processing pipeline defined in a
YAML file.

```bash
doppler compose init tone fir specan --name my_pipeline --out my_pipeline.yaml
doppler compose up my_pipeline.yaml
doppler ps
doppler logs my_pipeline
```

See [CLI & Pipelines](cli/index.md) and [Dopplerfile](cli/dopplerfile.md)
for writing custom blocks.

______________________________________________________________________

## Build from source

Only need the C **library** itself (headers + `libdoppler.a`/`.so`, no
examples, no Rust FFI, no toolchain)? `jbx get-doppler` — see
[Get it!](#get-it) above — is faster. This section is for the examples,
the Rust FFI bindings, running the test suite, or contributing.

!!! tip "Don't have `jbx` yet?"

    `make install-deps` bootstraps it for you (installs system build
    dependencies too). Or by hand:
    `. <(curl -sSL https://just-buildit.github.io/get-jb.sh)`.

```bash
git clone https://github.com/doppler-dsp/doppler
cd doppler
make install-deps  # bootstrap jbx (if needed) + install system deps
make               # C library + examples
make pyext         # Python extensions
make test-all      # C + Python + Rust test suites
```

You'll need a C compiler — your system's default one is enough, no C++
toolchain is required anywhere in the build (the core library and the
optional stream component, which vendors `nats.c`, are both pure C99).

??? note "Installing system deps by hand instead"

    `make install-deps` reads [`jb.toml`](https://github.com/doppler-dsp/doppler/blob/main/jb.toml),
    the single source of truth for doppler's system deps, so it stays in
    sync automatically. To install them yourself instead:

    === "Ubuntu / Debian"

        ```bash
        sudo apt-get install build-essential cmake pkg-config python3-dev python3-numpy
        ```

    === "Arch"

        ```bash
        sudo pacman -S --needed base-devel cmake python python-numpy
        ```

    === "Fedora / RHEL"

        ```bash
        sudo dnf install gcc make cmake pkgconf-pkg-config python3-devel python3-numpy
        ```

    === "openSUSE"

        ```bash
        sudo zypper install gcc make cmake pkg-config python3-devel python3-numpy
        ```

    === "macOS"

        ```bash
        brew install cmake python numpy
        ```

    === "Windows"

        doppler does not target Windows natively — build under
        [WSL2](https://learn.microsoft.com/windows/wsl/), a VM, or a
        container and follow the Ubuntu / Debian steps.

See [Build from Source](install/source.md) for CMake options, Docker, and
platform-specific notes.

______________________________________________________________________

## Next steps

- [Architecture](architecture.md) — design overview and layer diagram
- [Examples](examples/index.md): [C](examples/c.md) · [Streaming](examples/streaming.md)
- [API reference](c-api/files.md) — full C and Python API docs
- [Waveform Generator (wfmgen)](guide/wfmgen/index.md) — scenes, BLUE/SigMF,
    NATS streaming, and the Plan sweep cache
- [Spectrum Analyzer](specan/index.md) — specan configuration
- [CLI & Pipelines](cli/index.md) — compose and Dopplerfile
