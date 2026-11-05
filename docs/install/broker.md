# Run a NATS broker

doppler's streaming layer (publish/subscribe, push/pull, request/reply of IQ),
`wfmgen --output nats://…`, `doppler-specan --source socket` and the
streaming examples all talk to a **NATS server**. doppler is the client: it
does not start one, and nothing installs one for you. This page gets one
running in a minute. Run it in its own terminal and leave it up.

The default endpoint every page uses is `nats://127.0.0.1:4222/<subject>`:
the port is the server's, the last part is the subject you publish and
subscribe on.

## Start one

Pick whichever fits your machine.

=== "Docker (any OS)"

    ```sh
    docker run --rm -p 4222:4222 nats:2 -js
    ```

    `-js` turns JetStream on, which the docs' commands assume. Stop it with
    Ctrl-C; `--rm` removes the container.

=== "Debian / Ubuntu"

    ```sh
    sudo apt install nats-server
    nats-server -js
    ```

    The package puts the binary in `/usr/sbin`. Checked on Ubuntu 24.04 (2.10)
    and Debian stable.

=== "macOS, or anything else"

    On macOS, `brew install nats-server` (not checked here). For any other
    system, download a binary from [nats.io/download](https://nats.io/download/)
    and run it as `nats-server -js`.

The server is up when it prints `Server is ready`.

## Check it from Python

With the wheel installed (`pip install doppler-dsp`), this publishes eight
samples and reads them back, and prints `broker OK` when the broker works:

<!-- docs-snippet: broker=runs against the broker this page starts -->

```python
import numpy as np
from doppler.stream import CF32, Publisher, Subscriber

sub = Subscriber("nats://127.0.0.1:4222/check")
pub = Publisher("nats://127.0.0.1:4222/check", CF32)
x = np.arange(8).astype(np.complex64)
for _ in range(20):  # up to 5 s: the subscription may not have registered yet
    pub.send(x)
    try:
        samples, header = sub.recv(timeout_ms=250)
        break
    except TimeoutError:
        pass
else:
    raise SystemExit("no samples in 5 s: is a broker listening on :4222?")
print("broker OK:", len(samples), "samples")
```

If it fails with `dp_sub_create failed on nats://127.0.0.1:4222/…`, nothing is
listening on that port: start the server above, or point the endpoint at the
machine that is running it.

The loop publishes again until a sample arrives because a subscriber cannot yet
wait for the server to register its subscription, so a publish sent at once can
be lost ([#2865](https://github.com/doppler-dsp/doppler/issues/2865)). The same
is true of your own code: start the receiver first, and expect to lose what is
sent in its first moments.

## Where it is used

- the [Python quick start](../quickstart.md#streaming) and the
    [C streaming examples](../examples/streaming.md);
- [`wfmgen --output nats://…`](../guide/wfm-io/writing.md) and the
    [real-time guide](../guide/timing.md);
- [`doppler-specan --source socket`](../specan/index.md);
- the [Docker](docker.md) page, which bundles its own broker instead.
