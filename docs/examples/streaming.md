# Streaming Examples

## C: PUB/SUB

### Transmitter

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

### Receiver

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

## C: PUSH/PULL pipeline

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

For complete, runnable examples see [c/examples/](../../c/examples/).

---

## Python: Publisher / Subscriber

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

## Python: Push / Pull pipeline

```python
from doppler import Push, Pull, CF64
import numpy as np

samples = np.ones(512, dtype=np.complex128)

with Push("tcp://*:5556", CF64) as push:
    push.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Pull("tcp://localhost:5556") as pull:
    data, hdr = pull.recv(timeout_ms=500)
```

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
