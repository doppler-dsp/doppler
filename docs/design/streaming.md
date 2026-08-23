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
have no Python face today (gap 4 in §10).

______________________________________________________________________

## 3. The wire envelope

Every message is:

```text
+--------------------------------+-------------------------+
| dp_header_t  (96 bytes)        | payload (n * itemsize)  |
+--------------------------------+-------------------------+
```

Field by field, in declaration order — which is also byte order, because
the struct is memcpy'd whole and has no padding on any LP64 ABI:

| Offset | Size | Field          | Value                            |
| ------ | ---- | -------------- | -------------------------------- |
| 0      | 4    | `magic`        | `0x53494753` — "SIGS"            |
| 4      | 4    | `version`      | `0x00010000` (see below)         |
| 8      | 4    | `protocol`     | 0 = SIGS; 1 = DIFI, reserved     |
| 12     | 4    | `stream_id`    | 0 for SIGS                       |
| 16     | 4    | `sample_type`  | `dp_sample_type_t`               |
| 20     | 4    | `flags`        | bit 0 = `DP_FLAG_CHUNKED` (§4)   |
| 24     | 8    | `sequence`     | per-socket, starts at 0          |
| 32     | 8    | `timestamp_ns` | `CLOCK_REALTIME` ns              |
| 40     | 8    | `sample_rate`  | Hz, `double`                     |
| 48     | 8    | `center_freq`  | Hz, `double`                     |
| 56     | 8    | `num_samples`  | samples in *this message*        |
| 64     | 32   | `reserved[4]`  | chunk geometry when chunked (§4) |

**`version` on the wire is `0x00010000`, not `1`.** `stream.h` documents
the field as "Protocol version (currently 1)"; the value actually written
is `DP_VERSION` from `stream_internal.h`. A receiver comparing against 1
rejects every doppler frame. The header comment is the thing that is
wrong, and it is tracked as part of gap 1 in §10.

### Byte order and the CF128 trap

The header is **host byte order**. There is no conversion on either side —
`memcpy` out, `memcpy` in. Every architecture doppler ships for is
little-endian, so this has never bitten; a big-endian peer would read
garbage and the magic check would catch it.

Endianness is not the portability hazard worth worrying about. **CF128
is.** Its wire size is `sizeof(long double _Complex)`, which is 32 bytes
on both x86-64 and aarch64 — but the *representation* differs: x86-64
stores an 80-bit extended value in 16 bytes (`LDBL_MANT_DIG` 64), aarch64
stores IEEE binary128 (`LDBL_MANT_DIG` 113). The sizes agree, so nothing
rejects the frame, and the values decode to nonsense. doppler publishes
multi-arch container images, so the two ends of a CF128 stream can
genuinely differ. Use CF64 across an arch boundary.

### Payload layout per sample type

| Type        | Wire        | Payload                 | numpy dtype       |
| ----------- | ----------- | ----------------------- | ----------------- |
| `CI32` = 0  | 8 B/sample  | interleaved I,Q `int32` | `int32`, len `2n` |
| `CF64` = 1  | 16 B/sample | `double _Complex`       | `complex128`      |
| `CF128` = 2 | 32 B/sample | `long double _Complex`  | `clongdouble`     |
| `CI8` = 3   | 2 B/sample  | interleaved I,Q `int8`  | `int8`, len `2n`  |
| `CI16` = 4  | 4 B/sample  | interleaved I,Q `int16` | `int16`, len `2n` |
| `CF32` = 5  | 8 B/sample  | `float _Complex`        | `complex64`       |
| `TLM16` = 6 | 16 B/record | packed `dp_tlm_rec_t`   | structured        |

Values are fixed and appended, never renumbered — a new type must not
change what an older receiver already decodes. `CI32 = 0` rather than a
sentinel is a historical accident, so **zero is a valid sample type** and
a zeroed header is not distinguishable from a CI32 one by that field
alone. The magic is the check.

`TLM16` is not I/Q: `num_samples` counts 16-byte telemetry records, and
it is Publisher-only (no PUSH/REQ/REP variant exists in C). Its producer
side is `dp_tlm_sink_*`; see [Telemetry](telemetry.md).

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
`DP_FLAG_CHUNKED` set, all sharing the logical frame's `sequence`, and
use `reserved[]` for the geometry:

| Index | Name           | Meaning                                   |
| ----- | -------------- | ----------------------------------------- |
| 0     | `DP_CHUNK_IDX` | 0-based chunk number                      |
| 1     | `DP_CHUNK_CNT` | chunks in this frame                      |
| 2     | `DP_CHUNK_TOT` | total samples in the logical frame        |
| 3     | `DP_CHUNK_OFF` | this chunk's byte offset into the payload |

Chunks are sample-aligned — the split is `max_payload` rounded down to a
whole number of samples — so no sample straddles two messages.

Reassembly accepts a chunk only if the magic matches, the chunked flag is
set, the `sequence` equals the frame being assembled, and `DP_CHUNK_CNT`
agrees. A repeat of an index already seen is a no-op, so redelivery is
harmless. When the last chunk lands, the receiver is handed **one clean
logical frame**: doppler-owned buffer, `num_samples` set to the total,
`DP_FLAG_CHUNKED` cleared, `reserved[]` zeroed. A subscriber never sees a
chunk, which is why nothing in the Python API mentions them.

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
fails immediately — see gap 3 in §10.

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
ownership detail; see gap 5 in §10.

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

## 9. What this layer does not promise

- **No ordering across senders**, and no global ordering — only a
    per-socket sequence.
- **No exactly-once.** PUB/SUB is at-most-once; PUSH/PULL is
    at-least-once, so a duplicate after a redelivery is normal and a
    consumer that must not double-count needs its own dedupe on
    `(sender, sequence)`.
- **No cross-endian and no cross-ABI CF128** (§3).
- **No authentication or TLS configuration.** The endpoint parser accepts
    `nats://` only and passes the authority to the client verbatim, so
    URL-embedded credentials reach the server, but there is no
    creds-file, JWT, or TLS-options path — and no comma-separated cluster
    seed list, so a client points at one server (or a load-balanced
    address) rather than a seed set.
- **No role checking.** All six handle types are `typedef struct dp_ctx`,
    so in C nothing stops `dp_sub_recv()` on a publisher; see gap 2.
- **No backpressure on PUB.** A slow subscriber drops. That is the tier's
    property, not a defect — choose PUSH/PULL when loss is unacceptable.

______________________________________________________________________

## 10. Known gaps

Each is filed; this table is the map, not the detail.

| #   | Gap                                                                                                                                             | Issue                                                     |
| --- | ----------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| 1   | The public header documents `flags`/`reserved[]` as "do not interpret" and `version` as 1, so §3 and §4 cannot be derived from `stream.h` alone | [#958](https://github.com/doppler-dsp/doppler/issues/958) |
| 2   | Six handle types are one type; no entry point validates its role                                                                                | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
| 3   | `Pull` cannot create the work-queue stream, so a worker started first fails on a fresh broker                                                   | [#956](https://github.com/doppler-dsp/doppler/issues/956) |
| 4   | The raw-bytes control plane has no Python face                                                                                                  | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
| 5   | `Pull.ack(samples)` makes the caller hold a message lifetime                                                                                    | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
| 6   | Stale "only CI32/CF64/CF128 are supported" prose in the stub and the test module                                                                | [#958](https://github.com/doppler-dsp/doppler/issues/958) |
| 7   | `CI8` and `CI16` have no round-trip test                                                                                                        | [#958](https://github.com/doppler-dsp/doppler/issues/958) |
| 8   | Sample-type validation is ordinal (`> CF32`), not a predicate                                                                                   | [#958](https://github.com/doppler-dsp/doppler/issues/958) |
| 9   | Enum constants are unprefixed in a public header (`CF32`, `CI16`, …)                                                                            | [#958](https://github.com/doppler-dsp/doppler/issues/958) |
| 10  | `Push.send`'s size ceiling is undocumented on the Python face                                                                                   | [#959](https://github.com/doppler-dsp/doppler/issues/959) |
