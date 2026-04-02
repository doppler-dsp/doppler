# Doppler — Examples

## C: NCO

### Free-running IQ

```c
#include <dp/nco.h>
#include <stdio.h>

int main(void) {
    dp_nco_t *nco = dp_nco_create(0.25f);  // quarter-rate tone

    dp_cf32_t out[8];
    dp_nco_execute_cf32(nco, out, 8);

    for (int i = 0; i < 8; i++)
        printf("out[%d]: %.3f + %.3fi\n", i, out[i].i, out[i].q);
    // out[0]:  1.000 + 0.000i
    // out[1]:  0.000 + 1.000i
    // out[2]: -1.000 + 0.000i
    // out[3]:  0.000 - 1.000i
    // out[4]:  1.000 + 0.000i  (repeats every 4 samples)
    // ...

    dp_nco_destroy(nco);
    return 0;
}
```

### Raw uint32 phase + overflow carry

```c
dp_nco_t *nco = dp_nco_create(0.25f);

uint32_t phase[16];
uint8_t  carry[16];
dp_nco_execute_u32_ovf(nco, phase, carry, 16);
// carry fires at indices 3, 7, 11, 15 (once per full cycle)

dp_nco_destroy(nco);
```

### FM modulation via control port

```c
dp_nco_t *nco = dp_nco_create(0.1f);   // base freq f_n = 0.1

float ctrl[1024];
for (int i = 0; i < 1024; i++)
    ctrl[i] = 0.002f * sinf(2.0f * M_PI * 0.01f * i);  // FM deviation

dp_cf32_t out[1024];
dp_nco_execute_cf32_ctrl(nco, ctrl, out, 1024);
// base freq unchanged; reset restores clean phase
dp_nco_destroy(nco);
```

---

## C: FIR filter

```c
#include <dp/fir.h>
#include <math.h>

#define N_TAPS 19

int main(void) {
    // Windowed-sinc low-pass filter (fc = 0.2 * fs)
    dp_cf32_t taps[N_TAPS];
    int half = N_TAPS / 2;
    for (int k = 0; k < N_TAPS; k++) {
        int    n    = k - half;
        double sinc = (n == 0) ? 1.0
                                : sin(M_PI * 0.2 * n) / (M_PI * 0.2 * n);
        double win  = 0.5 * (1.0 - cos(2.0 * M_PI * k / (N_TAPS - 1)));
        taps[k].i = (float)(sinc * win);
        taps[k].q = 0.0f;
    }

    dp_fir_t *fir = dp_fir_create(taps, N_TAPS);

    // CF32 input
    dp_cf32_t in[1024], out[1024];
    dp_fir_execute_cf32(fir, in, out, 1024);

    // CI16 input (SDR-style 16-bit IQ)
    dp_ci16_t in16[1024];
    dp_fir_execute_ci16(fir, in16, out, 1024);

    dp_fir_destroy(fir);
    return 0;
}
```

---

## C: FFT

### 1D FFT (out-of-place)

```c
#include <doppler.h>
#include <dp/fft.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    dp_init();

    const size_t N = 1024;
    size_t shape[] = {N};
    dp_fft_global_setup(shape, 1, -1, 1, "estimate", NULL);

    double complex in[N], out[N];
    for (size_t i = 0; i < N; i++)
        in[i] = cos(2.0 * M_PI * 10.0 * i / N) + 0.0 * I;

    dp_fft1d_execute(in, out);
    printf("DC bin: %.4f + %.4fi\n", creal(out[0]), cimag(out[0]));

    dp_cleanup();
    return 0;
}
```

### 1D FFT (in-place)

```c
dp_fft1d_execute_inplace(data);  // modifies data in-place
```

### 2D FFT

```c
size_t shape[] = {64, 64};
dp_fft_global_setup(shape, 2, -1, 1, "estimate", NULL);
dp_fft2d_execute(in2d, out2d);
dp_fft2d_execute_inplace(in2d);
```

---

## C: SIMD arithmetic

```c
#include <dp/util.h>
#include <complex.h>
#include <stdio.h>

int main(void) {
    double complex a = 1.0 + 2.0 * I;
    double complex b = 3.0 + 4.0 * I;
    double complex c = dp_c16_mul(a, b);
    printf("result: %.1f + %.1fi\n", creal(c), cimag(c));  // -5.0 + 10.0i
    return 0;
}
```

---

## C: Streaming

### PUB/SUB — transmitter

```c
#include <dp/stream.h>
#include <math.h>

int main(void) {
    dp_pub *tx = dp_pub_create("tcp://*:5555", DP_CF64);

    dp_cf64_t samples[1000];
    for (int i = 0; i < 1000; i++) {
        double phase = 2.0 * M_PI * 1000.0 * i / 1e6;
        samples[i].i = cos(phase);
        samples[i].q = sin(phase);
    }

    dp_pub_send_cf64(tx, samples, 1000, 1e6, 2.4e9);
    dp_pub_destroy(tx);
    return 0;
}
```

### PUB/SUB — receiver

```c
#include <dp/stream.h>
#include <stdio.h>

int main(void) {
    dp_sub *rx = dp_sub_create("tcp://localhost:5555");

    dp_msg_t *msg;
    dp_header_t hdr;

    dp_sub_recv(rx, &msg, &hdr);
    printf("Received %zu samples, type=%s\n",
           dp_msg_num_samples(msg),
           dp_sample_type_str(dp_msg_sample_type(msg)));

    dp_msg_free(msg);
    dp_sub_destroy(rx);
    return 0;
}
```

### PUSH/PULL pipeline

```c
#include <dp/stream.h>
#include <stdio.h>

// Producer (binds)
dp_push *push = dp_push_create("ipc:///tmp/pipe.ipc", DP_CF64);
dp_push_send_cf64(push, samples, count, 1e6, 2.4e9);
dp_push_destroy(push);

// Consumer (connects)
dp_pull *pull = dp_pull_create("ipc:///tmp/pipe.ipc");
dp_msg_t *msg;
dp_header_t hdr;
dp_pull_recv(pull, &msg, &hdr);
dp_cf64_t *data = (dp_cf64_t *)dp_msg_data(msg);
// use data ...
dp_msg_free(msg);
dp_pull_destroy(pull);
```

For complete, runnable examples see [c/examples/](../c/examples/).

---

## Python: NCO

```python
from doppler import Nco
import numpy as np

# Free-running quarter-rate tone
with Nco(0.25) as nco:
    iq = nco.execute_cf32(8)
    print(iq)
    # [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j  1.+0.j  0.+1.j -1.+0.j  0.-1.j]

# Raw phase + overflow carry
with Nco(0.25) as nco:
    ph, carry = nco.execute_u32_ovf(16)
    # carry is 1 at indices 3, 7, 11, 15

# FM control port
ctrl = 0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024)).astype(np.float32)
with Nco(0.1) as nco:
    iq = nco.execute_cf32_ctrl(ctrl)

# Phase continuity: split calls resume where the last left off
with Nco(0.25) as nco:
    a = nco.execute_cf32(4)
    b = nco.execute_cf32(4)   # seamlessly continues from sample 4
```

---

## Python: FFT

### One-shot FFT

```python
from doppler.fft import fft
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
spectrum = fft(x)
print(f"FFT result: {len(spectrum)} bins")
```

### Reusing a plan (faster for repeated transforms)

```python
from doppler.fft import setup, execute1d, execute1d_inplace
import numpy as np

x = np.random.randn(1024) + 1j * np.random.randn(1024)
setup(x.shape, nthreads=4, planner="measure")  # plan once

for _ in range(1000):
    out = execute1d(x)             # out-of-place, returns new array
    # execute1d_inplace(x)         # or modify x in-place
```

### 2D FFT (Python)

```python
from doppler.fft import setup, execute2d
import numpy as np

x = np.random.randn(64, 64) + 1j * np.random.randn(64, 64)
setup(x.shape, nthreads=1, planner="estimate")
out = execute2d(x)
```

---

## Python: Streaming

### Publisher / Subscriber

```python
from doppler import Publisher, Subscriber, CF64
import numpy as np

samples = np.array([1+2j, 3+4j, 5+6j], dtype=np.complex128)

with Publisher("tcp://*:5555", CF64) as pub:
    pub.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Subscriber("tcp://localhost:5555") as sub:
    data, hdr = sub.recv(timeout_ms=500)
    print(f"Got {hdr.num_samples} samples, seq={hdr.sequence}")
```

### Push / Pull pipeline

```python
from doppler import Push, Pull, CF64
import numpy as np

samples = np.ones(512, dtype=np.complex128)

with Push("tcp://*:5556", CF64) as push:
    push.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Pull("tcp://localhost:5556") as pull:
    data, hdr = pull.recv(timeout_ms=500)
```

### Circular buffers

High-performance double-mapped ring buffers for producer/consumer pipelines:

```python
from doppler import F32Buffer, F64Buffer, I16Buffer
import numpy as np
import threading

# F64 (double-complex) — minimum 256 samples (page-alignment constraint)
buf = F64Buffer(256)

def producer():
    data = np.ones(256, dtype=np.complex128)
    buf.write(data)   # non-blocking; returns False on overflow

def consumer():
    view = buf.wait(256)   # blocks until 256 samples available
    process(view)          # zero-copy view into buffer memory
    buf.consume(256)       # release for reuse

threading.Thread(target=consumer).start()
threading.Thread(target=producer).start()
```

| Type | NumPy dtype | Min samples | Notes |
| ---- | ----------- | ----------- | ----- |
| `F32Buffer` | `complex64` | 512 | float IQ pairs |
| `F64Buffer` | `complex128` | 256 | double IQ pairs |
| `I16Buffer` | `int16, shape=(n,2)` | 1024 | col 0 = I, col 1 = Q |

---

## Network configurations

### Single machine (localhost)

```c
// Transmitter
dp_pub_create("tcp://*:5555", DP_CF64);

// Receiver (same machine)
dp_sub_create("tcp://localhost:5555");
```

```bash
# Run the examples
./build/transmitter
./build/receiver
```

### Two machines over LAN

**Step 1:** Find the transmitter machine's IP address:

```bash
# On the transmitter machine:
ip addr show | grep inet
# or:
hostname -I
```

**Step 2:** Open the firewall port on the transmitter:

```bash
# On the transmitter machine:
sudo ufw allow 5555/tcp
sudo ufw status
```

**Step 3:** Run the transmitter (binds to all interfaces):

```c
// Transmitter code — binds to *all* network interfaces
dp_pub_create("tcp://*:5555", DP_CF64);
```

```bash
# On Machine A (transmitter):
./build/transmitter
```

**Step 4:** Run the receiver with the transmitter's IP:

```c
// Receiver code — connects to specific IP
dp_sub_create("tcp://192.168.1.100:5555");
```

```bash
# On Machine B (receiver) — replace with transmitter's actual IP:
./build/receiver tcp://192.168.1.100:5555
```

### Local IPC (fastest, same machine only)

```c
dp_pub_create("ipc:///tmp/doppler.ipc", DP_CF64);
dp_sub_create("ipc:///tmp/doppler.ipc");
```

### Docker Compose

```yaml
services:
  tx:
    command: /app/transmitter tcp://*:5555 cf64
  rx:
    command: /app/receiver tcp://tx:5555  # uses Docker DNS
```

---

## Troubleshooting

### Receiver can't connect to transmitter

**Symptom:** Receiver hangs at "Waiting for packets..." when running on a different machine.

**Solution:**

1. **Verify the receiver is using the correct IP:**
   ```bash
   # On the receiver machine, check you're connecting to the transmitter's IP:
   ./build/receiver tcp://192.168.1.100:5555
   # NOT tcp://localhost:5555 (that's the receiver's own machine!)
   ```

2. **Check network connectivity:**
   ```bash
   # From the receiver machine:
   ping 192.168.1.100           # verify basic connectivity
   nc -zv 192.168.1.100 5555    # test if port 5555 is reachable
   # or:
   telnet 192.168.1.100 5555
   ```

3. **Verify the firewall on the transmitter:**
   ```bash
   # On the transmitter machine:
   sudo ufw status              # check if port 5555 is allowed
   sudo ufw allow 5555/tcp      # open it if needed
   ```

4. **Check for cloud/network firewalls:**
   - AWS security groups, Azure NSGs, GCP firewall rules
   - Router port forwarding if crossing networks

### Packets are being dropped

**Symptom:** Dashboard shows "Dropped: N" packets.

**Causes & solutions:**

- **Slow joiner problem:** Receiver started after transmitter began sending
  - Solution: Start receiver first, or wait for transmitter's 1-second startup delay

- **Network congestion:** Too much data for the link
  - Solution: Reduce sample rate or packet size in transmitter code

- **Receiver is too slow:** Processing can't keep up with arrival rate
  - Solution: Profile your receiver code, optimize processing

### "Address already in use" error

**Symptom:** Transmitter fails with `zmq_bind: Address already in use`

**Solutions:**

1. **Kill the old process:**
   ```bash
   # Find the process using port 5555:
   sudo lsof -i :5555
   # or:
   sudo netstat -tulpn | grep 5555

   # Kill it:
   kill <PID>
   ```

2. **Use a different port:**
   ```bash
   ./build/transmitter tcp://*:5556
   ./build/receiver tcp://192.168.1.100:5556
   ```

3. **Wait for ZMQ socket cleanup:** Sometimes sockets take a few seconds to release after Ctrl+C.

### No output / silent failure

**Enable ZMQ debug logging:**

```bash
export ZMQ_VERBOSE=1
./build/transmitter
```

**Check library paths (Linux):**

```bash
# Verify libdoppler.so is found:
ldd ./build/transmitter
ldd ./build/receiver

# If missing, set LD_LIBRARY_PATH:
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Performance tips

- **Use IPC for same-machine communication:** Much faster than TCP localhost
- **Batch samples:** Larger packets = less overhead (but more latency)
- **Disable Nagle's algorithm:** For low-latency, use `tcp://` with ZMQ_TCP_NODELAY
- **Pin threads to cores:** For real-time processing (see `pthread_setaffinity_np`)

### Getting help

If you're still stuck:

1. Check existing issues: https://github.com/doppler-dsp/doppler/issues
2. Include in your bug report:
   - Output of `./build/transmitter --help` and `./build/receiver --help`
   - Network topology (same machine, LAN, cloud, containers)
   - Error messages with `ZMQ_VERBOSE=1`
   - OS and library versions (`uname -a`, `cmake --version`, `pkg-config --modversion libzmq`)
