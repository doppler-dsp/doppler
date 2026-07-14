# Streaming Examples

## C: PUB/SUB

Requires a running `nats-server` (e.g. `nats-server -js`).

`dp_pub_*`/`dp_sub_*` live in the **optional stream archive**, not the
core library — an `undefined reference to dp_pub_create` means
`libdoppler_stream.a` is missing from the link line. Compile any snippet
on this page with both archives (either order works; the stream layer is
pure C and adds only `-lpthread`):

```sh
cc app.c -I "$HOME/.local/doppler/include" \
   "$HOME/.local/doppler/lib/libdoppler_stream.a" \
   "$HOME/.local/doppler/lib/libdoppler.a" \
   -lm -lpthread -o app
```

### Transmitter

<!-- docs-snippet: broker=publishes to a live broker; CI's python-tests job provides one, compile-checked everywhere -->

```c
#include <stream/stream.h>
#include <complex.h>
#include <math.h>

int main(void) {
    dp_pub_t *tx = dp_pub_create("nats://127.0.0.1:4222/iq", CF64);

    double _Complex samples[1000];
    for (int i = 0; i < 1000; i++) {
        double phase = 2.0 * M_PI * 1000.0 * i / 1e6;
        samples[i] = cos(phase) + sin(phase) * _Complex_I;
    }

    dp_pub_send_cf64(tx, samples, 1000, 1e6, 2.4e9);
    dp_pub_destroy(tx);
    return 0;
}
```

### Receiver

<!-- docs-snippet: no-run=blocking recv needs a live transmitter; compile-checked against the real wire API, round-trip covered by stream tests -->

```c
#include <stream/stream.h>
#include <stdio.h>

int main(void) {
    dp_sub_t *rx = dp_sub_create("nats://127.0.0.1:4222/iq");

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

The PUSH/PULL work-queue tier is backed by NATS JetStream, so pushed
frames survive a restart of the consumer and are load-balanced
round-robin across every connected `Pull`.

<!-- docs-snippet: skip=illustrative excerpt (undeclared samples/count), needs a live broker; see examples/c/pipeline_demo for the tested version -->

```c
#include <stream/stream.h>
#include <stdio.h>

// Producer
dp_push_t *push = dp_push_create("nats://127.0.0.1:4222/work", CF64);
dp_push_send_cf64(push, samples, count, 1e6, 2.4e9);
dp_push_destroy(push);

// Consumer
dp_pull_t *pull = dp_pull_create("nats://127.0.0.1:4222/work");
dp_msg_t *msg;
dp_header_t hdr;
dp_pull_recv(pull, &msg, &hdr);
dp_cf64_t *data = (dp_cf64_t *)dp_msg_data(msg);
// use data ...
dp_msg_free(msg);
dp_pull_destroy(pull);
```

For complete, runnable examples see [`examples/c/`](https://github.com/doppler-dsp/doppler/tree/main/examples/c).

______________________________________________________________________

## Python: Publisher / Subscriber

Requires a running `nats-server` (e.g. `nats-server -js`).

<!-- docs-snippet: skip=blocking NATS recv, needs a broker; covered by stream tests -->

```python
from doppler.stream import Publisher, Subscriber, CF64
import numpy as np

samples = np.array([1+2j, 3+4j, 5+6j], dtype=np.complex128)

with Publisher("nats://127.0.0.1:4222/iq", CF64) as pub:
    pub.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Subscriber("nats://127.0.0.1:4222/iq") as sub:
    data, hdr = sub.recv(timeout_ms=500)
    print(f"Got {hdr['num_samples']} samples, seq={hdr['sequence']}")
```

For a complete runnable example with live dashboard and graceful shutdown:

```bash
python src/doppler/examples/transmitter.py nats://127.0.0.1:4222/iq
python src/doppler/examples/receiver.py nats://127.0.0.1:4222/iq
```

## Python: Push / Pull pipeline

The PUSH/PULL work-queue tier is backed by NATS JetStream: pushed
frames are durably queued and load-balanced round-robin across every
connected `Pull` worker.

<!-- docs-snippet: skip=blocking NATS recv, needs a broker; covered by stream tests -->

```python
from doppler.stream import Push, Pull, CF64
import numpy as np

samples = np.ones(512, dtype=np.complex128)

# Multiple Pull workers share frames round-robin via a JetStream
# work-queue consumer group.
with Push("nats://127.0.0.1:4222/work", CF64) as push:
    push.send(samples, sample_rate=1e6, center_freq=2.4e9)

with Pull("nats://127.0.0.1:4222/work") as pull:
    data, hdr = pull.recv(timeout_ms=500)
    print(f"Got {hdr['num_samples']} samples at {hdr['sample_rate'] / 1e6:.2f} MHz")
```

Run multiple workers for parallel processing:

```bash
# Terminal 1 — sender
python src/doppler/examples/pipeline_send.py nats://127.0.0.1:4222/work

# Terminals 2 and 3 — two parallel workers
python src/doppler/examples/pipeline_recv.py nats://127.0.0.1:4222/work 0
python src/doppler/examples/pipeline_recv.py nats://127.0.0.1:4222/work 1
```

## Python: Requester / Replier

REQ/REP models a remote DSP service: the client sends a signal block,
the server processes it and returns the result. The exchange is strictly
alternating — `send` then `recv` on the Requester, `recv` then `send` on
the Replier.

<!-- docs-snippet: skip=two-process NATS REQ/REP, needs a broker; covered by stream tests -->

```python
from doppler.stream import Requester, Replier, CF64
import numpy as np

ep = "nats://127.0.0.1:4222/ctrl"

# Server side — run in a thread or separate process
with Replier(ep, CF64) as rep:
    request, hdr = rep.recv(timeout_ms=5000)
    result = request * 0.5                    # example: apply -6 dB gain
    rep.send(result, sample_rate=hdr["sample_rate"])

# Client side
x = np.ones(1024, dtype=np.complex128)
with Requester(ep, CF64) as req:
    req.send(x, sample_rate=int(1e6), center_freq=int(2.4e9))
    reply, hdr = req.recv(timeout_ms=2000)
    print(f"Reply: {len(reply)} samples, seq={hdr['sequence']}")
```

Complete standalone examples:

```bash
# Terminal 1 — server (start first)
python src/doppler/examples/replier.py nats://127.0.0.1:4222/ctrl --gain 0.5

# Terminal 2 — client
python src/doppler/examples/requester.py nats://127.0.0.1:4222/ctrl --count 20
```

______________________________________________________________________

## Network configurations

Every doppler stream endpoint is a NATS client — the transmitter and the
receiver both *connect* to a `nats-server` broker (`nats://host:port/subject`);
neither one binds a listening socket, so "transmitter" and "receiver" are
peers of the broker rather than of each other.

### Single machine (localhost)

```bash
# Start a broker once, in its own terminal:
nats-server -js
```

<!-- docs-snippet: skip=illustrative fragment (no main, return values discarded); see the Transmitter/Receiver sections above for the tested versions -->

```c
// Transmitter and receiver both connect to the local broker
dp_pub_create("nats://127.0.0.1:4222/iq", CF64);
dp_sub_create("nats://127.0.0.1:4222/iq");
```

```bash
# Run the examples
./build/examples/c/transmitter
./build/examples/c/receiver
```

### Two machines over LAN

The broker needs to be reachable from both the transmitter and the
receiver — run it on either machine (or a third) and point both clients
at its address.

**Step 1:** Find the broker machine's IP address:

```bash
# On the broker machine:
ip addr show | grep inet
# or:
hostname -I
```

**Step 2:** Open the firewall port on the broker machine:

```bash
# On the broker machine:
sudo ufw allow 4222/tcp
sudo ufw status
```

**Step 3:** Start the broker, listening on all interfaces:

```bash
# On the broker machine:
nats-server -js -a 0.0.0.0
```

**Step 4:** Point both the transmitter and the receiver at the broker's IP:

```bash
# On Machine A (transmitter) — replace with the broker's actual IP:
./build/examples/c/transmitter nats://192.168.1.100:4222/iq

# On Machine B (receiver) — same broker IP:
./build/examples/c/receiver nats://192.168.1.100:4222/iq
```

### Loopback (fastest, same machine only)

NATS is TCP-only — there is no unix-domain-socket transport — so the
fastest same-machine path is still a loopback connection to a broker on
`127.0.0.1`:

<!-- docs-snippet: skip=illustrative fragment (no main, return values discarded); see the Transmitter/Receiver sections above for the tested versions -->

```c
dp_pub_create("nats://127.0.0.1:4222/iq", CF64);
dp_sub_create("nats://127.0.0.1:4222/iq");
```

### Docker Compose

```yaml
services:
  nats:
    image: nats:latest
    command: ["-js"]
    ports:
      - "4222:4222"
  tx:
    command: /app/transmitter nats://nats:4222/iq cf64
    depends_on: [nats]
  rx:
    command: /app/receiver nats://nats:4222/iq  # uses Docker DNS
    depends_on: [nats]
```

______________________________________________________________________

## Troubleshooting

### Receiver can't connect to the broker

**Symptom:** Receiver hangs at "Waiting for packets..." when the broker is on a different machine.

**Solution:**

1. **Verify the receiver is using the broker's IP, not its own:**

    ```bash
    # On the receiver machine, check you're connecting to the broker's IP:
    ./build/examples/c/receiver nats://192.168.1.100:4222/iq
    # NOT nats://127.0.0.1:4222/iq (that's the receiver's own machine!)
    ```

1. **Check network connectivity:**

    ```bash
    # From the receiver machine:
    ping 192.168.1.100            # verify basic connectivity
    nc -zv 192.168.1.100 4222     # test if the broker's port is reachable
    # or:
    telnet 192.168.1.100 4222
    ```

1. **Verify the firewall on the broker machine:**

    ```bash
    # On the broker machine:
    sudo ufw status              # check if port 4222 is allowed
    sudo ufw allow 4222/tcp      # open it if needed
    ```

1. **Check for cloud/network firewalls:**

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

### "No server available for connection" error

**Symptom:** Transmitter or receiver fails immediately with a NATS
`No server available for connection` status — there is no `nats-server`
listening at the endpoint's host:port.

**Solutions:**

1. **Start (or restart) the broker:**

    ```bash
    nats-server -js
    ```

1. **Verify it's actually listening on the port you expect:**

    ```bash
    # Find the process bound to port 4222:
    sudo lsof -i :4222
    # or:
    sudo netstat -tulpn | grep 4222
    ```

1. **Use a different port** if something else already owns 4222:

    ```bash
    nats-server -js -p 4223
    ./build/examples/c/transmitter nats://192.168.1.100:4223/iq
    ./build/examples/c/receiver nats://192.168.1.100:4223/iq
    ```

### No output / silent failure

**Check library paths (Linux):**

```bash
# Verify libdoppler.so is found:
ldd ./build/examples/c/transmitter
ldd ./build/examples/c/receiver

# If missing, set LD_LIBRARY_PATH:
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Performance tips

- **Run the broker on the same machine for the lowest latency:** a loopback
    `nats://127.0.0.1:4222/…` connection beats a cross-machine one.
- **Batch samples:** Larger packets = less overhead (but more latency)
- **Pin threads to cores:** For real-time processing (see `pthread_setaffinity_np`)

### Getting help

If you're still stuck:

1. Check existing issues: https://github.com/doppler-dsp/doppler/issues
1. Include in your bug report:
    - Output of `./build/examples/c/transmitter --help` and `./build/examples/c/receiver --help`
    - Network topology (same machine, LAN, cloud, containers)
    - `nats-server` logs (run it in the foreground, without `-js` daemonizing, to see connection attempts)
    - OS and toolchain versions (`uname -a`, `cmake --version`) — note doppler statically embeds the vendored `nats.c` client, so there is no system NATS client library to query
