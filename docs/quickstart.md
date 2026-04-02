# Quick Start

## Install

```bash
pip install doppler-dsp
```

That's it. The wheel bundles all native dependencies — no system
libraries required.

Optional extras:

```bash
pip install doppler-specan    # live spectrum analyzer web UI
pip install doppler-cli       # compose / Dopplerfile pipeline CLI
```

---

## Signal processing

### NCO — generate a complex tone

```python
from doppler import Nco

with Nco(0.25) as nco:        # normalised frequency: 0.25 → Fs/4
    iq = nco.execute_cf32(8)
    print(iq)
    # [1.+0.j  0.+1.j  -1.+0.j  0.-1.j ...]
```

### FFT

```python
from doppler.fft import fft
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
X = fft(x)                    # complex spectrum, same length
```

### FIR filter

```python
from doppler import FirFilter
import numpy as np

filt = FirFilter.lowpass_cf32(cutoff=0.1, num_taps=63)
y = filt.execute(x.astype(np.complex64))
```

### Resample

```python
from doppler.resample import HalfbandDecimator

decim = HalfbandDecimator()
y = decim.execute(x.astype(np.complex64))  # 2:1 decimation
```

---

## Streaming

Doppler streams IQ data over ZMQ. Transmit and receive on the same
machine or across a network — the API is identical.

### Publisher (Python)

```python
from doppler.stream import Publisher
import numpy as np

pub = Publisher("tcp://*:5555")

samples = np.ones(1024, dtype=np.complex64)
pub.send(samples, sample_rate=1e6, center_freq=2.4e9)
```

### Subscriber (Python)

```python
from doppler.stream import Subscriber

sub = Subscriber("tcp://localhost:5555")

msg = sub.recv()
print(f"Received {len(msg.samples)} samples @ {msg.sample_rate/1e6:.1f} MHz")
```

### C transmitter → Python subscriber

Build the C examples once, then mix and match:

```bash
make          # builds ./build/c/transmitter, receiver, etc.

# Terminal 1
./build/c/transmitter tcp://*:5555 cf32

# Terminal 2 (Python)
python - <<'EOF'
from doppler.stream import Subscriber
sub = Subscriber("tcp://localhost:5555")
while True:
    msg = sub.recv()
    print(f"seq={msg.seq}  samples={len(msg.samples)}")
EOF
```

---

## Spectrum analyzer

`doppler-specan` opens a live FFT display in your browser.

**Demo mode** (no hardware needed):

```bash
doppler-specan --source demo
```

**From a live IQ stream:**

```bash
doppler-specan --source socket --endpoint tcp://localhost:5555
```

The web UI is served at `http://127.0.0.1:8765` by default.
See [Spectrum Analyzer](specan/index.md) for configuration options.

---

## Pipeline CLI

`doppler compose` wires blocks into a processing pipeline defined in a
YAML file.

```bash
# Create a named pipeline file
doppler compose init --name my_pipeline

# Edit my_pipeline.yaml, then start it
doppler compose up --file my_pipeline.yaml

# Monitor running blocks
doppler ps
doppler logs
```

See [CLI & Pipelines](cli/index.md) and [Dopplerfile](cli/dopplerfile.md)
for writing custom blocks.

---

## Build from source

If you need the C library, examples, or Rust FFI bindings:

**Dependencies:**

=== "Ubuntu / Debian"

    ```bash
    sudo apt-get install \
        build-essential cmake pkg-config \
        libzmq3-dev libfftw3-dev python3-dev
    ```

=== "macOS"

    ```bash
    brew install cmake zeromq fftw
    ```

=== "Windows (MSYS2 UCRT64)"

    ```bash
    pacman -S mingw-w64-ucrt-x86_64-gcc \
              mingw-w64-ucrt-x86_64-cmake \
              mingw-w64-ucrt-x86_64-zeromq \
              mingw-w64-ucrt-x86_64-fftw make
    ```

**Build:**

```bash
git clone https://github.com/doppler-dsp/doppler
cd doppler
make                      # C library + examples
make pyext                # Python extensions (requires uv)
make test-all             # C + Python + Rust test suites
```

See [Build & Install](build.md) for CMake options, Docker, and
platform-specific notes.

---

## Next steps

- [Overview](overview.md) — architecture and full API reference
- [Examples](examples/index.md) — C, Python, and streaming examples
- [API reference](api/python-fft.md) — full Python API docs
- [Spectrum Analyzer](specan/index.md) — specan configuration
- [CLI & Pipelines](cli/index.md) — compose and Dopplerfile
