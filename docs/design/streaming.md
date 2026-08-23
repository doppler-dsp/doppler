# Streaming — one envelope, six roles, two planes

doppler's transport moves framed sample blocks between threads, processes,
and hosts over NATS. Every frame — whatever the role, whatever the sample
type — is one 96-byte `dp_header_t` followed by raw payload bytes, so a C
transmitter and a Python subscriber are the same participant seen twice.

This page owns the **contract**: what is on the wire, which subjects carry
it, who owns which buffer, and what the layer does not promise. It is the
page to read before writing a receiver that is not doppler's own.

- Calling the API: [Python Streaming API](../api/python-streaming.md) and
    [`native/inc/stream/stream.h`](../c-api/stream_8h.md)
- Runnable both-sides walk-throughs, network topologies, troubleshooting:
    [Streaming examples](../examples/streaming.md)
- The telemetry payload that rides the same envelope:
    [Telemetry](telemetry.md)

Historical rationale — why NATS at all, and the ZMQ-era benchmarks that
decided it — is kept in
[the streaming roadmap](../dev/archive/streaming-roadmap.md) and
[the transport migration](../dev/archive/nats-jetstream-transport-migration.md).
Both are archives: ZMQ is gone, and NATS is the sole transport.

______________________________________________________________________

## 1. Two planes, one envelope

The layer serves two workloads whose figures of merit have nothing to do
with each other, and the split explains almost every design choice below.

|                | **I/Q firehose**                   | **status / control**   |
| -------------- | ---------------------------------- | ---------------------- |
| carries        | sample blocks                      | small messages, Hz–kHz |
| metric         | throughput, sustained              | delivery semantics     |
| roles          | PUSH/PULL, PUB/SUB                 | REQ/REP, PUB/SUB       |
| tolerates loss | a display can; a work queue cannot | no                     |

What both need is the *same* framing, because the same block flows from a
firehose into a control reply without being re-encoded. So there is one
`dp_header_t` and one magic, and the plane shows up as a choice of role,
not as a second format.

The cost is stated rather than hidden: every send copies. NATS takes one
contiguous buffer, and the header and payload are separate allocations
until then, so `nats_publish_framed` mallocs `header + payload` and
memcpys both. Receive is zero-copy; send is not, and cannot be without a
scatter-gather transport.

______________________________________________________________________

## 2. Roles and tiers

Six roles, three patterns, two NATS tiers:

| Role | C handle    | Python       | NATS tier    | Delivery                    |
| ---- | ----------- | ------------ | ------------ | --------------------------- |
| PUB  | `dp_pub_t`  | `Publisher`  | core         | at-most-once, fan-out       |
| SUB  | `dp_sub_t`  | `Subscriber` | core         | lossy on a slow consumer    |
| PUSH | `dp_push_t` | `Push`       | JetStream    | server-acked, persisted     |
| PULL | `dp_pull_t` | `Pull`       | JetStream    | at-least-once, explicit ack |
| REQ  | `dp_req_t`  | `Requester`  | core req/rep | lock-step                   |
| REP  | `dp_rep_t`  | `Replier`    | core req/rep | lock-step                   |

PUB/SUB and REQ/REP need only `nats-server`. PUSH/PULL ride the JetStream
work-queue tier and need `nats-server -js`.

Each role additionally carries a **raw-bytes** face in C —
`dp_req_send` / `dp_req_recv` / `dp_rep_send` / `dp_rep_recv` take a
`void *` and a byte count instead of a typed sample block, for control
messages that are not signals. Those are the status plane proper; they
have no Python face today (gap 3 in §10).

______________________________________________________________________

## 3. The wire envelope

Every message is:

```text
+------------------------+-------------------+------------------------+
| dp_header_t (64 bytes) | dp_chunk_t (24)?  | payload (n * elem)     |
+------------------------+-------------------+------------------------+
                          only when CHUNKED
```

Field by field, in declaration order — which is also byte order, because
the struct is memcpy'd whole. `stream_core.c` carries a `_Static_assert`
on the size and on every offset below, so the layout is a build failure
away from drifting rather than a comment.

| Offset | Size | Field           | Value                                   |
| ------ | ---- | --------------- | --------------------------------------- |
| 0      | 8    | `magic`         | `DP_STREAM_MAGIC` — `DPSTREAM` as u64   |
| 8      | 4    | `data_rep`      | `"EEEI"` (LE) or `"IEEE"` (BE)          |
| 12     | 2    | `format`        | BLUE code; 0 when the kind is not I/Q   |
| 14     | 2    | `kind`          | `dp_frame_kind_t` — what the payload IS |
| 16     | 2    | `version`       | `DP_WIRE_VERSION` = 2                   |
| 18     | 2    | `flags`         | bit 0 = `DP_FLAG_CHUNKED` (§4)          |
| 20     | 4    | `payload_bytes` | payload length THIS message claims      |
| 24     | 8    | `sequence`      | per-socket, from 0                      |
| 32     | 8    | `timestamp_ns`  | `CLOCK_REALTIME` ns, or 0 = unset       |
| 40     | 8    | `sample_rate`   | Hz, `double`                            |
| 48     | 8    | `center_freq`   | Hz, `double`                            |
| 56     | 8    | `num_samples`   | samples or records in THIS message      |

**The magic is an integer, not a `char[8]`, and that is load-bearing.**
The header is host byte order with no conversion, so a peer of the
opposite endianness reads the magic byte-swapped and it stops matching —
the format tag is therefore also the endianness probe, for free. A
`char[8]` would read identically either way and detect nothing.
`data_rep` then says *which* order it was, using BLUE's own token so a
hex dump answers the question without decoding anything.

### What a receiver checks

Five things, before it hands anything back:

1. `magic` — is this ours, in our byte order;
1. `version` — a different major is refused, not guessed at;
1. `flags` — any bit outside `DP_FLAG_KNOWN` is refused, because an
    unknown block would move where the payload starts;
1. `payload_bytes` against the message length the transport reports —
    the header's claim must equal the transport's truth;
1. `num_samples × element size` against that same length.

Checks 4 and 5 are the reason the field exists. v1 validated the magic
and nothing else, so a header claiming more samples than its message
carried produced an out-of-bounds read on both faces — the numpy array
was sized from the header's own claim, over a buffer nobody measured.

### Formats are BLUE codes, and that is the point

The value in `format` is the two-character Midas BLUE 1.1 Table 6 code,
packed little-endian — the same two characters the same samples get at
HCB bytes 52/53 when written to a file.

| Format | Code   | Wire        | Payload                 | numpy dtype       |
| ------ | ------ | ----------- | ----------------------- | ----------------- |
| `CI8`  | `"CB"` | 2 B/sample  | interleaved I,Q `int8`  | `int8`, len `2n`  |
| `CI16` | `"CI"` | 4 B/sample  | interleaved I,Q `int16` | `int16`, len `2n` |
| `CI32` | `"CL"` | 8 B/sample  | interleaved I,Q `int32` | `int32`, len `2n` |
| `CF32` | `"CF"` | 8 B/sample  | `float _Complex`        | `complex64`       |
| `CF64` | `"CD"` | 16 B/sample | `double _Complex`       | `complex128`      |

doppler used to hold **three** enumerations of these five types — the
stream's own, `wfm_writer`'s `stype` in "wavegen order", and
`wfm_sink.c`'s `WT_*` — agreeing on no single value, plus a `FMTCH[]`
table mapping one of them to BLUE and a `BPS[]` table restating the
sizes. Every boundary between them was a hand-written switch. Naming a
format by the code the file format already defines leaves one vocabulary
and nothing to translate, so the codes live in `native/inc/dp_format.h`
(a header, all `static inline`) rather than in either container: the
transport is the wrong thing to link in order to name a file's samples.

There is no code for a quad or extended float. BLUE defines none, which
is the file format independently reaching the conclusion that retired
CF128 from the wire — its representation differs between x86-64 and
aarch64 at identical size, so a frame crossed an architecture boundary,
matched every field a receiver checks, and decoded to nonsense.

### A telemetry frame is a kind, not a format

`TLM16` used to be a sixth sample type. It is not a sample encoding: its
payload is 16-byte `dp_tlm_rec_t` records, `num_samples` counts records,
and only a Publisher can emit it. As a sample type it forced every
I/Q-only sender to carry an exception for it and left `format` holding a
value BLUE does not define.

So the frame says what it is in `kind` (`DP_KIND_IQ` / `DP_KIND_TLM`),
`format` stays purely a BLUE sample code, and a telemetry frame sets it
to 0. In C a telemetry publisher has its own constructor
(`dp_pub_create_tlm`) because there is no format to pass; in Python it
stays `Publisher(ep, TLM16)`, since "what does this socket publish" is
one question to a caller and the two vocabularies cannot collide — every
BLUE code is two ASCII characters packed into 16 bits, so all of them
are ≥ 0x4200, while `DP_KIND_TLM` is 1.

Its producer side is `dp_tlm_sink_*`; see [Telemetry](telemetry.md).

______________________________________________________________________

## 4. Chunking

A NATS server caps message size (1 MiB by default; the connection reports
its own limit, which the context caches at create time). A CF64 frame of
100 k samples is 1.6 MB and does not fit.

**Chunking is a PUB/SUB feature only, deliberately.** Every subscriber
receives the whole in-order chunk sequence and reassembles independently.
Over a load-balanced work queue the chunks of one frame could land on
different workers, so PUSH does not chunk: an oversized frame returns
`DP_ERR_TOO_LARGE` (-7) rather than being silently split.

A chunked frame's messages each carry a full header with
`DP_FLAG_CHUNKED` set, all sharing the logical frame's `sequence`,
followed by a 24-byte `dp_chunk_t`:

| Offset | Size | Field         | Meaning                            |
| ------ | ---- | ------------- | ---------------------------------- |
| 0      | 4    | `index`       | 0-based chunk number               |
| 4      | 4    | `count`       | chunks in this frame               |
| 8      | 8    | `total_bytes` | payload bytes in the whole frame   |
| 16     | 8    | `offset`      | this chunk's byte offset into that |

It rides only chunked frames. v1 spent a third of its 96-byte header on
four `reserved[]` words carrying exactly this, zeroed on every unchunked
frame — and documented as "set to zero, do not interpret", which meant
the format could not be implemented from the header doppler publishes.

Chunks are element-aligned — the split is `max_payload` minus the header
and chunk block, rounded down to a whole number of elements — so no
sample straddles two messages.

Reassembly accepts a chunk only if it passes every check in §3, carries
`DP_FLAG_CHUNKED`, has the `sequence` of the frame being assembled, and
agrees on `count`. A repeat of an index already seen is a no-op, so
redelivery is harmless. When the last chunk lands, the receiver is handed
**one clean logical frame**: doppler-owned buffer, `num_samples` and
`payload_bytes` set to the totals, `DP_FLAG_CHUNKED` cleared. A
subscriber never sees a chunk, which is why nothing in the Python API
mentions them.

Two consequences worth stating plainly:

- **One publisher per subject, if frames may chunk.** Reassembly reads
    the next messages on the subscription until the frame completes; a
    message from a *different* publisher arriving mid-frame fails the
    sequence check and the whole frame is dropped with `DP_ERR_INVALID`.
- **A lost chunk loses its frame**, not just its own bytes. Core NATS is
    at-most-once, so a slow subscriber that drops one chunk of a 4-chunk
    frame gets nothing for that frame.

______________________________________________________________________

## 5. Subjects and JetStream objects

An endpoint is `nats://host:port/<base>`. The authority becomes the
server URL verbatim; `<base>` is a **name, not a subject** — the roles
derive their own subjects from it, and an endpoint with no path uses the
base `default`.

| Role | Subject                                             |
| ---- | --------------------------------------------------- |
| PUB  | publishes `iq.<base>.<type>` (e.g. `iq.iq.cf64`)    |
| SUB  | subscribes `iq.<base>.>` — every type on that base  |
| PUSH | publishes `work.<base>.<type>` via JetStream        |
| PULL | pulls the durable consumer below                    |
| REQ  | publishes `<base>` with a reply inbox               |
| REP  | subscribes `<base>`, replies to the request's inbox |

The type token in the subject is what lets a subscriber take a wildcard
and still decode per message: the authority on a frame's type is its
header, and the subject merely allows broker-side filtering by type if a
consumer wants it.

JetStream objects, both derived from `<base>`:

| Object   | Name             | Config                                                              |
| -------- | ---------------- | ------------------------------------------------------------------- |
| Stream   | `DP_WORK_<base>` | subjects `work.<base>.>`, work-queue retention, file storage, `R=1` |
| Consumer | `DP_PULL_<base>` | durable, explicit ack, manual ack by the caller                     |

`R=1` is a development default; a production deployment pre-provisions
the stream with `R=3` before any doppler process connects, and
`nats_ensure_stream` then adopts the existing stream as-is rather than
reconfiguring it.

**The Push side creates the stream; the Pull side only binds it.** So on
a broker that has never carried `DP_WORK_<base>`, starting a worker first
fails immediately — see gap 2 in §10.

______________________________________________________________________

## 6. Ownership and lifetime

Receive is zero-copy, and that has a lifetime consequence the API makes
explicit rather than hiding.

`*_recv` yields an opaque `dp_msg_t *`. `dp_msg_data()` points **into**
the NATS message (past the 96-byte header) for an unchunked frame, or to
a doppler-owned buffer for a reassembled one — `dp_msg_free()` is correct
for both. The data is valid until that free, and not after.

In Python the returned numpy array holds the reference: the array *is*
the message handle's owner, and the buffer lives until the array is
collected. Which is also why `Pull.ack()` takes the array back —
acknowledging a JetStream delivery needs the underlying message, and the
array is what is holding it. It works, and it makes the caller carry a C
ownership detail; see gap 4 in §10.

Acks are the work queue's contract, not a formality: delivery is
at-least-once, a frame stays pending until acked, and a worker that dies
before acking gets its frame redelivered to another worker.

______________________________________________________________________

## 7. Sequence and time

`sequence` counts **per socket**, from 0, incremented on every logical
frame (a chunked frame consumes one number, not one per chunk). It is
therefore a *sender-side* counter: a gap at a subscriber means that
subscriber dropped frames, and two publishers on one subject produce two
interleaved sequences that no receiver can reconcile.

`timestamp_ns` is `CLOCK_REALTIME` nanoseconds, stamped at send. A hop
that re-publishes upstream data should not restamp it — the origin time
is what a latency measurement or a downstream time base needs — so the
senders take an override (`dp_ctx_set_timestamp_ns` in C,
`timestamp_ns=` in Python) that applies to the next send and then clears
itself. The same clock is exposed as `dp_get_timestamp_ns()` /
`get_timestamp_ns()` so a receiver can subtract.

______________________________________________________________________

## 8. Connection behaviour

- **Reconnect is on and unbounded** — infinite retries, 100 ms apart,
    with an 8 MiB reconnect buffer. A broker restart is survivable; the
    frames published into the buffer while disconnected are not
    guaranteed.
- **Subscriptions have unbounded pending limits.** A subscriber that
    stops calling `recv()` grows the client's pending queue rather than
    dropping early — memory is the backstop, which suits a DSP consumer
    that stalls briefly and recovers.
- **Timeouts are per receiver.** `dp_*_set_timeout` (`timeout_ms=` in
    Python) sets a millisecond deadline; the default is to block.
    `DP_ERR_TIMEOUT` (-5) is a normal, expected return, and Python raises
    `TimeoutError`.
- **`max_payload` is discovered, not assumed** — read from the connection
    at create time, with a 1 MiB fallback if the server reports nothing.

______________________________________________________________________

## 8b. Stopping a blocking receive

A blocking `*_recv` waits inside the NATS client. A flag your signal
handler sets is read by your loop — which the blocking call is keeping
you out of. With traffic arriving that is invisible, because every frame
returns control to you; the moment a sender stops, Ctrl+C stops working.

That is not hypothetical. doppler's own C receiver example shipped with
it: 0.00 s to exit while the transmitter ran, indefinite once it stopped.

Two answers, and the layer supports both:

- **Bound the wait.** `*_set_timeout` plus treating `DP_ERR_TIMEOUT` as
    "loop round and re-read my flag". Simple, and it makes every caller
    trade latency against responsiveness.
- **Interrupt it.** `dp_stream_interrupt()` sets a process-wide flag that
    the library checks *inside* the wait, so a blocking receive stays
    blocking and still returns — with `DP_ERR_INTERRUPTED`, which is a
    request to stop rather than a failure. It assigns to a
    `volatile sig_atomic_t` and does nothing else, so a signal handler
    may call it. `dp_stream_interrupt_on_signal()` installs such a handler
    for you; `dp_stream_resume()` clears the flag.

Internally every wait is sliced at 100 ms, which is what bounds how long
an interrupt appears to be ignored — ten wakeups a second on an idle
subscriber, and a delay no human perceives. The flag is checked *before*
the first slice too, so a receive started after the signal refuses
immediately rather than parking once: interrupt-then-receive and
receive-then-interrupt behave the same.

The flag is **sticky**. A handler fires once and the loops it unblocks
may be several, so an auto-clearing flag would release one caller and
leave the rest parked.

**In Python the handler must be installed from C, and that is not an
implementation detail.** A Python handler runs when the interpreter next
regains control, which is precisely what the blocking wait prevents — the
flag would be set only after the wait it is meant to end. Measured: a
first version of `interrupt_on_sigint()` used `signal.signal` and left a
blocked `recv()` blocked forever. So `interrupt_on_sigint()` installs a
`sigaction` and **chains** to whatever was there, including CPython's own
handler; without the chaining, a Ctrl+C arriving outside a receive would
set a flag nobody reads, fixing the blocking case by breaking the
ordinary one.

An unblocked `recv()` raises `KeyboardInterrupt` — the exception Ctrl+C
raises anywhere else, so a caller's existing `try`/`finally` and `with`
cleanup applies unchanged. It is raised **once**: the binding calls
`PyErr_CheckSignals()` first and lets CPython raise its own pending one
if there is one, because raising ours as well delivered a second
`KeyboardInterrupt` into the caller's cleanup block.

______________________________________________________________________

## 9. What this layer does not promise

- **No ordering across senders**, and no global ordering — only a
    per-socket sequence.
- **No exactly-once.** PUB/SUB is at-most-once; PUSH/PULL is
    at-least-once, so a duplicate after a redelivery is normal and a
    consumer that must not double-count needs its own dedupe on
    `(sender, sequence)`.
- **No cross-endian frames** (§3). The magic detects it; nothing
    converts.
- **No optional-block negotiation.** A receiver refuses a frame carrying
    a flag bit it does not know rather than guessing where the payload
    starts, which is what makes a later additive block (BLUE keywords is
    the candidate) safe to introduce inside major version 2.
- **No authentication or TLS configuration.** The endpoint parser accepts
    `nats://` only and passes the authority to the client verbatim, so
    URL-embedded credentials reach the server, but there is no
    creds-file, JWT, or TLS-options path — and no comma-separated cluster
    seed list, so a client points at one server (or a load-balanced
    address) rather than a seed set.
- **No role checking.** All six handle types are `typedef struct dp_ctx`,
    so in C nothing stops `dp_sub_recv()` on a publisher; see gap 1.
- **No backpressure on PUB.** A slow subscriber drops. That is the tier's
    property, not a defect — choose PUSH/PULL when loss is unacceptable.

______________________________________________________________________

## 10. Known gaps

Each is filed; this table is the map, not the detail. Three more stood
here and are closed: the stale "only three sample types" prose and the
ordinal type check went with the CF128 retirement, and the public header
now documents the flags and the chunk block, because v2 made them real
fields instead of a `reserved[]` the format secretly used.

| #   | Gap                                                                                           | Issue                                                     |
| --- | --------------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| 1   | Six handle types are one type; no entry point validates its role                              | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
| 2   | `Pull` cannot create the work-queue stream, so a worker started first fails on a fresh broker | [#956](https://github.com/doppler-dsp/doppler/issues/956) |
| 3   | The raw-bytes control plane has no Python face                                                | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
| 4   | `Pull.ack(samples)` makes the caller hold a message lifetime                                  | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
| 5   | `CI8` and `CI16` have no round-trip test                                                      | [#962](https://github.com/doppler-dsp/doppler/issues/962) |
| 6   | Format names are unprefixed in a public header (`CF32`, `CI16`, …)                            | [#962](https://github.com/doppler-dsp/doppler/issues/962) |
| 7   | `Push.send`'s size ceiling is undocumented on the Python face                                 | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
