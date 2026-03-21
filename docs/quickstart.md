# doppler — Quick Start

## Option 1: Docker (recommended for fastest setup)

```bash
# Build the image (~130 MB)
docker build -t doppler .

# Run tests
docker run --rm doppler /app/test_stream
```

Run the full streaming stack with docker compose:

```bash
# Transmitter + 2 receivers + spectrum analyzer
docker compose up
```

Or run individual containers (two terminals):

```bash
# Terminal 1: transmitter (publishes on port 5555)
docker run --rm -p 5555:5555 doppler /app/transmitter tcp://*:5555 cf64

# Terminal 2: receiver (connects to host port)
docker run --rm --network host doppler /app/receiver tcp://localhost:5555
```

## Option 2: Native build

### Dependencies

**Ubuntu/Debian:**

```bash
sudo apt-get install build-essential libzmq3-dev libfftw3-dev cmake python3 python3-dev
```

**macOS:**

```bash
brew install zeromq fftw cmake python3
python3 -m pip install --break-system-packages numpy
```

### Build

```bash
make
```

Then install the Python package:

```bash
uv sync
```

See [Build Guide](build.md) for CMake options and platform-specific notes.

---

## Signal processing in Python

```python
from doppler.fft import fft
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
spectrum = fft(x)
print(f"FFT bins: {len(spectrum)}")
```

---

## Streaming demo

### Single machine (3 terminals)

```bash
# Terminal 1: transmitter (binds to all interfaces)
./build/transmitter tcp://*:5555 cf64

# Terminal 2: receiver (connects to localhost)
./build/receiver tcp://localhost:5555

# Terminal 3: spectrum analyzer
./build/spectrum_analyzer tcp://localhost:5555 512
```

### Two machines over LAN

**Machine A (transmitter):**

```bash
# Find your IP address
ip addr show | grep inet  # or: hostname -I

# Open firewall (if needed)
sudo ufw allow 5555/tcp

# Start transmitter (binds to all interfaces)
./build/transmitter tcp://*:5555 cf64
```

**Machine B (receiver):**

```bash
# Connect to Machine A's IP (replace 192.168.1.100 with actual IP)
./build/receiver tcp://192.168.1.100:5555

# Or run spectrum analyzer
./build/spectrum_analyzer tcp://192.168.1.100:5555 512
```

**Troubleshooting:** If the receiver hangs at "Waiting for packets...", verify:
1. You're using the transmitter's IP, not `localhost`
2. Port 5555 is open: `nc -zv 192.168.1.100 5555`
3. Firewall allows the connection: `sudo ufw status`

See [Examples](examples.md#troubleshooting) for detailed troubleshooting.

---

## Next steps

- [Examples](examples.md) — C and Python code examples
- [Overview](overview.md) — Architecture and full API reference
- [Build Guide](build.md) — CMake options, platform notes, Docker details
