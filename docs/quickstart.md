# Quick Start

## Install

```bash
pip install doppler-dsp
```

The wheel bundles all native dependencies — no system libraries required.

!!! tip "Optional extras"

    ```bash
    pip install "doppler-dsp[specan]"      # terminal spectrum analyzer
    pip install "doppler-dsp[specan-web]"  # live spectrum analyzer web UI
    pip install "doppler-dsp[cli]"         # compose / Dopplerfile pipeline CLI
    ```

    See [Install → Python](install/python.md) for the full extras table.

______________________________________________________________________

## Signal processing

Every object is a thin, stateful wrapper over the C core: construct it once,
then stream blocks through it. The examples below build on each other — the
signal `x` created in the FFT step is reused by the filter and resampler.

### LO — generate a complex tone

```python
from doppler.source import LO

lo = LO(0.25)          # normalised frequency: 0.25 → Fs/4
iq = lo.steps(8)
print(iq)
# [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j ...]
```

### FFT

```python
from doppler.spectral import FFT
import numpy as np

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
fft = FFT(1024)
X = fft.execute_cf32(x)   # complex64 in → complex64 out (~2× faster than f64)
```

### FIR filter

```python
from doppler.filter import FIR
from scipy.signal import firwin

taps = firwin(63, cutoff=0.1, window="hamming").astype(np.float32)
fir = FIR(taps)
y = fir.execute(x)        # reuses x from the FFT step
```

### Resample

`RateConverter` picks the cheapest cascade (halfband / CIC / polyphase) for
the rate you ask for — no filter design required:

```python
from doppler.resample import RateConverter

rc = RateConverter(0.5)   # 2:1 decimation → auto-selects a halfband stage
y = rc.execute(x)         # 1024 → 512 samples
```

______________________________________________________________________

## Streaming

Doppler streams IQ data over ZMQ. Transmit and receive on the same
machine or across a network — the API is identical.

### Publisher (Python)

<!-- docs-snippet: skip=two-terminal ZMQ demo; the send/recv round-trip and message fields are gated by doppler/stream/tests/test_stream.py -->

```python
from doppler.stream import Publisher
import numpy as np

pub = Publisher("tcp://*:5555")

samples = np.ones(1024, dtype=np.complex64)
pub.send(samples, sample_rate=1e6, center_freq=2.4e9)
```

### Subscriber (Python)

<!-- docs-snippet: skip=blocking recv() with no peer; round-trip covered by doppler/stream/tests/test_stream.py -->

```python
from doppler.stream import Subscriber

sub = Subscriber("tcp://localhost:5555")

msg = sub.recv()
print(f"Received {len(msg.samples)} samples @ {msg.sample_rate/1e6:.1f} MHz")
```

### C transmitter → Python subscriber

Build the C examples once, then mix and match:

```bash
make          # builds ./build/examples/c/transmitter, receiver, etc.

# Terminal 1
./build/examples/c/transmitter tcp://*:5555 cf32

# Terminal 2 (Python)
python - <<'EOF'
from doppler.stream import Subscriber
sub = Subscriber("tcp://localhost:5555")
while True:
    msg = sub.recv()
    print(f"seq={msg.seq}  samples={len(msg.samples)}")
EOF
```

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
doppler compose init --name my_pipeline
doppler compose up --file my_pipeline.yaml
doppler ps
doppler logs
```

See [CLI & Pipelines](cli/index.md) and [Dopplerfile](cli/dopplerfile.md)
for writing custom blocks.

______________________________________________________________________

## Build from source

If you need the C library, examples, or Rust FFI bindings:

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
    sudo dnf install gcc gcc-c++ make cmake pkgconf-pkg-config python3-devel python3-numpy
    ```

=== "openSUSE"

    ```bash
    sudo zypper install gcc gcc-c++ make cmake pkg-config python3-devel python3-numpy
    ```

=== "macOS"

    ```bash
    brew install cmake python numpy
    ```

!!! info "Windows"

    doppler does not target Windows natively — build under
    [WSL2](https://learn.microsoft.com/windows/wsl/), a VM, or a container
    and follow the Ubuntu / Debian steps.

!!! note "C++ compiler"

    `gcc-c++` above is needed **only** for the optional ZMQ/stream component
    (the vendored libzmq is C++). The core C library and the rest of the
    Python package are pure C99 — drop it if you don't need streaming.

```bash
git clone https://github.com/doppler-dsp/doppler
cd doppler
make           # C library + examples
make pyext     # Python extensions
make test-all  # C + Python + Rust test suites
```

See [Build from Source](install/source.md) for CMake options, Docker, and
platform-specific notes.

______________________________________________________________________

## Next steps

- [Architecture](architecture.md) — design overview and layer diagram
- [Examples](examples/index.md): [C](examples/c.md) · [Streaming](examples/streaming.md)
- [API reference](c-api/files.md) — full C and Python API docs
- [Spectrum Analyzer](specan/index.md) — specan configuration
- [CLI & Pipelines](cli/index.md) — compose and Dopplerfile
