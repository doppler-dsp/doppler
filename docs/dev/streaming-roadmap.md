# Streaming roadmap: resilient transport for Kubernetes

**Status:** complete — NATS JetStream shipped (P1) and ZMQ has since been
fully removed from doppler; NATS is now the sole streaming transport. The
sections below are kept as the historical record of why NATS was added and
the benchmark data that justified it; where they recommend keeping ZMQ for
the co-located firehose (§8, §9.3), that recommendation was superseded by
the later decision to drop ZMQ entirely.
**Scope:** doppler's network streaming of I/Q (and metadata) — where it went
beyond the original ZeroMQ transport layer.

______________________________________________________________________

## 1. Context

doppler already ships **three ZMQ messaging patterns** via `libdoppler_stream`
(opt-in, vendored static libzmq — see `archive/EXTENSION_WITH_STATIC_ZMQ.md`):

| Pattern   | Sender      | Receiver    | Use case                |
| --------- | ----------- | ----------- | ----------------------- |
| PUB/SUB   | `dp_pub_*`  | `dp_sub_*`  | Fan-out broadcast       |
| PUSH/PULL | `dp_push_*` | `dp_pull_*` | Pipeline / load-balance |
| REQ/REP   | `dp_req_*`  | `dp_rep_*`  | Control / metadata      |

All three are exposed as Python handles (`Publisher`/`Subscriber`,
`Push`/`Pull`, `Requester`/`Replier`) and as C senders/receivers in
`native/inc/stream/stream.h`. The existing surface covers **six wire types**
(`CF32`, `CF64`, `CF128`, `CI8`, `CI16`, `CI32`) with a shared `dp_header_t`
envelope.

PUB/SUB is the current `ZmqSink` backing (wfmgen's `--output zmq://…`). It is
the *right* tool for a **co-located, low-latency I/Q firehose** — a live
spectrum display or an in-rack consumer that tolerates loss. It is the *wrong*
tool for an elastic, resilient **Kubernetes** deployment.

## 2. What actually breaks per pattern in k8s

| Pattern   | k8s gap                                                                                                                                                       |
| --------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| PUB/SUB   | Discovery (hard-coded `tcp://host:port`); reconnect after pod restart; backpressure (PUB silently **drops** to slow subscribers); no durability/replay; no HA |
| PUSH/PULL | Discovery only — backpressure and round-robin load-balance already work; a full PUSH queue blocks the sender                                                  |
| REQ/REP   | Discovery + reconnect; otherwise fine for low-rate control                                                                                                    |

PUSH/PULL already closes the "horizontal consumer scaling / work-sharing" gap
that brokerless PUB/SUB can't. The remaining gap for PUSH/PULL in k8s is
purely service discovery — pods are ephemeral, IPs churn, so a hard-coded
`bind tcp://host:port` breaks when a pod restarts.

## 3. Proposal: NATS JetStream as the *resilient* backend

[NATS JetStream](https://docs.nats.io/nats-concepts/jetstream) is
purpose-built for the k8s axis:

- **Subjects** decouple producers/consumers; discovery is *connect to the NATS
    cluster*, not to a peer IP. A producer publishes `iq.<stream>.<segment>`;
    consumers subscribe by subject — pods come and go freely.
- **JetStream streams** are append logs → **durability + replay**: a consumer
    that restarts resumes from its last ack (or replays from the start).
- **Consumers** (durable, pull-based, work-queue) give **horizontal scaling** —
    N replicas share a subject's load with at-least-once delivery.
- **Acks + flow control** give real **backpressure** instead of silent drops.
- **RAFT-replicated streams** give **HA**.
- **k8s-native**: official Helm chart, runs as a `StatefulSet` with PVCs for
    JetStream storage; clients connect via a `Service`
    (`nats://nats.ns.svc:4222`); leaf nodes for edge/ingest.

**Architectural bonus — it drops the C++.** The NATS C client
([nats.c](https://github.com/nats-io/nats.c)) is **pure C** (TLS via OpenSSL,
optional libsodium). A NATS backend therefore avoids the `stdc++` that the
vendored libzmq forces. That directly serves the C-first / C++-free-core goal.

### NATS equivalents per ZMQ pattern

| ZMQ pattern | NATS equivalent                                                        |
| ----------- | ---------------------------------------------------------------------- |
| PUB/SUB     | Core NATS pub/sub (ephemeral) or JetStream push consumer (durable)     |
| PUSH/PULL   | Core NATS queue group (ephemeral) or JetStream pull consumer (durable) |
| REQ/REP     | Core NATS request/reply (built-in, low-rate, no persistence needed)    |

## 4. Honest tradeoffs

NATS JetStream is **not a drop-in replacement** for the ZMQ firehose:

- **Latency:** a broker hop + (for JetStream) a persistence write per message
    — higher than raw brokerless ZMQ. Fine for control/metadata and moderate I/Q
    rates; measure before trusting it for tight closed loops.
- **Throughput ceiling:** sustained multi-GB/s raw-I/Q firehose is not what a
    broker is for. High sample rates need chunking + careful subject/stream
    design, or stay on a raw transport.
- **Message size:** JetStream messages are bounded (default 1 MB,
    configurable); continuous high-rate I/Q must be framed into bounded blocks
    (doppler already blocks the stream — align block size to a sane message
    limit).
- **Persistence cost:** disk + RAFT replication isn't free; use ephemeral (core
    NATS) where replay isn't needed.

**Conclusion:** add NATS as a backend, don't swap. Route by use case.

## 5. Recommended shape: a pluggable transport seam

doppler already has the seam (`stream/`) and the opt-in-component pattern
(`libdoppler_stream`, the weak-symbol stub). Generalize it:

- A **transport-backend interface** behind `stream/` (open / send-block /
    recv-block / close + an `*_available()` probe), with each backend a thin C
    wrapper. Each is an **opt-in component** so a deployment links only what it
    uses, and the core stays `-lm`-only.
- **Each backend is just another generated `kind="handle"`** — `NatsSink` /
    `NatsSource` mirror `ZmqSink` / `ZmqSource` exactly (same `open`/`send`/
    `close` binding shape). Adding a transport is ~a C wrapper + a manifest
    block, **zero new jm work**. (This is the payoff of the handle-generator
    adoption already underway.)
- **One wire envelope.** Reuse the `dp_header_t` framing (magic,
    `sample_type`, `fs`, `fc`, `num_samples`, timestamp) as the message body
    across *all* transports, so a ZMQ producer and a NATS consumer agree
    byte-for-byte. JetStream message **headers** carry the metadata; the subject
    (`iq.<stream>.<segment>`) carries the routing.

### Backend matrix

| ZMQ pattern | ZMQ component (today)   | NATS component (new)                       |
| ----------- | ----------------------- | ------------------------------------------ |
| PUB/SUB     | `libdoppler_stream_zmq` | `wfm_nats_pub.c` / `wfm_nats_sub.c`        |
| PUSH/PULL   | `libdoppler_stream_zmq` | `wfm_nats_push.c` / `wfm_nats_pull.c`      |
| REQ/REP     | `libdoppler_stream_zmq` | lowest priority — stay ZMQ or migrate last |

### Composition with in-flight work

- **SampleClock + the realtime-paced stream** compose with JetStream flow
    control: pace to `fs`, publish per block, let consumer acks throttle.
- **Durable capture / replay** = a JetStream stream + a SigMF sidecar; replay
    is a re-read of the stream — no separate capture path.

## 6. Phased plan

| Phase                         | Deliverable                                                                                                                                                                                                                                                               |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **P0 — envelope + benchmark** | Pin the wire envelope (`dp_header_t` framing + subject scheme); build a throughput/latency harness for **both PUB/SUB and PUSH/PULL** vs ZMQ at representative I/Q rates (CF32 at 1 MS/s, 10 MS/s, ~100 MS/s). *Gate the whole effort on these numbers.*                  |
| **P1 — NATS core**            | `wfm_nats_pub.c`/`sub.c` + `wfm_nats_push.c`/`pull.c` over nats.c (core pub/sub + queue groups, no persistence) → generated `NatsSink`/`NatsSource`/`NatsPush`/`NatsPull` handles; parity with ZMQ for both patterns; `libdoppler_stream_nats` opt-in component (pure C). |
| **P2 — JetStream + k8s**      | Durable streams, pull consumers, acks, flow control; Helm deploy (StatefulSet + PVC); document durable-consumer scaling + reconnection patterns.                                                                                                                          |
| **P3 — durability/replay/HA** | Durable capture + SigMF; replay; exactly-once (dedupe + double-ack); RAFT-replicated streams.                                                                                                                                                                             |
| **P4 — routing matrix**       | Decision doc: which transport for which use case (ZMQ firehose vs NATS resilient vs NATS pipeline), backed by the P0/P4 benchmark data.                                                                                                                                   |

## 7. Open questions to validate

1. **Throughput/latency at target sample rates** — the make-or-break for P1;
    decides the firehose-vs-resilient boundary for *both* PUB/SUB and PUSH/PULL.
1. **PUSH/PULL queue depth**: does a NATS pull consumer saturate a DSP-rate
    PUSH sender, or does the ack round-trip introduce unacceptable head-of-line
    blocking?
1. **Chunking** strategy for continuous high-rate I/Q within JetStream's
    message bound.
1. **Persistence** (disk/RAFT) vs ephemeral per use case.
1. **AuthN/Z + TLS** — NATS accounts/JWT vs the cluster's mesh;
    CURVE-equivalent.
1. **nats.c deps** (OpenSSL/libsodium) — vendor (like libzmq) or system?

## 8. Recommendation

- NATS JetStream is the right instinct for the **resilient / k8s** axis —
    built for it, and the **pure-C client is a genuine architectural win** (sheds
    the libzmq C++ dependency the core has been isolating).
- PUSH/PULL already closes the load-balance / backpressure gap locally; the
    only real k8s fix it needs is service discovery — which NATS subjects give
    for free.
- Keep a low-latency brokerless path (ZMQ, or raw UDP) for the co-located
    firehose. The choice is **per-use-case, behind the pluggable seam** — which
    the handle-generator adoption makes cheap.
- **Biggest risk** is throughput/latency at DSP rates for the PUSH/PULL path;
    commit only after the P0 benchmark. PUB/SUB resilience (durability, scaling,
    discovery) NATS clearly wins regardless.

**Recommendation: proceed to P0 (envelope + benchmark, both patterns) now;
greenlight P1+ on the numbers.**

______________________________________________________________________

## 9. P0 benchmark results

doppler runs **two planes** with different figures of merit, and P0 measures
each on its own terms:

- the **I/Q firehose** (PUSH/PULL) — high-rate sample blocks; the metric is
    **throughput** (§9.1);
- the **status / control / telemetry plane** (PUB/SUB + REQ/REP) — small
    messages at a low rate; the metric is **unloaded latency + delivery
    semantics**, *not* throughput (§9.2).

**Harness:** `native/benchmarks/bench_stream.c` — two pthreads (producer +
consumer) with a semaphore-pair start barrier (portable; macOS lacks
`pthread_barrier`), 50-frame warmup, 32k-bin 1 µs latency histogram. Run on a
single host (dev box, Release build). One-way / RTT latency uses
`dp_header_t.timestamp_ns` (`CLOCK_REALTIME`, valid because both threads share
the clock).

**Transport under test:** ZMQ PUSH/PULL over `tcp://127.0.0.1:5679` and ZMQ
REQ/REP over `tcp://127.0.0.1:5680`; NATS core pub/sub over
`nats://127.0.0.1:4222` (nats-server v2.14.2 in-process, no JetStream,
`nats bench pub + sub` CLI).

**Message payload:** firehose = CF32 I/Q (8 bytes/sample); status = a 64-byte
control message. No framing overhead subtracted.

> **Loopback overstates the brokerless edge for the *distributed* case.** At
> ≥ 4 k-sample frames both transports exceed 10 GbE line rate (~1,250 MB/s), so
> on a real NIC the firehose is link-bound for *either* transport and the
> throughput gap below collapses. The ZMQ advantage is a **co-located** (ipc /
> loopback) phenomenon — which is exactly the axis it's recommended for.

### 9.1 Firehose throughput (PUSH/PULL)

PUSH/PULL has **backpressure**: the producer blocks if the consumer falls
behind, so every frame is delivered exactly once and throughput is the true
end-to-end rate. One-way latency uses `dp_header_t.timestamp_ns`
(`CLOCK_REALTIME` sender → receiver; valid because both threads share the same
clock).

| block_sz (samples) | frame bytes | tput MS/s | tput MB/s | lat_mean µs | lat_p99 µs |
| ------------------ | ----------- | --------- | --------- | ----------- | ---------- |
| 256                | 2,048       | 110       | 840       | 964         | 1,069      |
| 1,024              | 8,192       | 199       | 1,520     | 1,072       | 1,362      |
| 4,096              | 32,768      | 461       | 3,516     | 832         | 1,135      |
| 16,384             | 131,072     | 563       | 4,299     | 2,725       | 5,088      |
| 65,536             | 524,288     | 721       | 5,501     | 5,671       | 11,974     |

**Caveats:**

- TCP loopback bypasses the NIC but still traverses the kernel TCP stack.
    Real network throughput will be lower; a 10 GbE link caps at ~1,250 MB/s.
- Latency is measured from `dp_push_send_cf32` → first byte past `dp_pull_recv`
    on the same machine. Cross-host latency needs PTP/NTP-synchronised clocks.
- No pinning, no `SO_BUSY_POLL`, no huge pages. Numbers are a pessimistic
    baseline; tuned production configs will be better.
- **The `lat_*` columns are *saturation* latency, not at-rate latency.** The
    producer runs flat-out into the default ZMQ send queue (`SNDHWM` ≈ 1000
    frames), so the ~1 ms means are dominated by **queue depth**, not transport
    cost — which is why they are non-monotonic in frame size (256 → 964 µs but
    4096 → 832 µs). They bound the firehose's worst case under saturation; the
    *unloaded* small-message latency that matters for control lives in §9.2.

### Firehose: NATS as a candidate (throughput upper bound)

This is still the **firehose** question — *can a NATS broker carry the I/Q
stream?* — measured against NATS's fastest path (core pub/sub, no persistence),
so it's an upper bound. It is **not** the status plane (that's §9.2). `nats bench pub` + `nats bench sub`, single publisher + subscriber, no JetStream.
Throughput is the **subscriber** msgs/sec (end-to-end) × actual frame size; the
CLI's MB/s assumes a fixed 128 B default and is ignored. `nats bench` latency is
**publish-to-broker** only — not end-to-end — and is not comparable.

| block_sz (samples) | frame bytes | NATS sub msgs/s | NATS MB/s | ZMQ MB/s | ZMQ / NATS |
| ------------------ | ----------- | --------------- | --------- | -------- | ---------- |
| 256                | 2,048       | 418,666         | 820       | 840      | 1.0×       |
| 1,024              | 8,192       | 139,061         | 1,084     | 1,520    | 1.4×       |
| 4,096              | 32,768      | 60,921          | 1,901     | 3,516    | 1.85×      |
| 16,384             | 131,072     | 16,424          | 2,049     | 4,299    | 2.1×       |
| 65,536             | 524,288     | 4,819           | 2,398     | 5,501    | 2.3×       |

**Caveats:**

- Core NATS routes every message through the broker (an extra copy + server
    CPU); ZMQ PUSH/PULL is socket-to-socket. The gap grows with frame size
    because the broker copy cost is proportional to bytes.
- At 256-sample frames the two are **essentially tied** (~820 vs 840 MB/s).
    The break-even is somewhere in the 256–1024 sample range.
- These are **core NATS** numbers — not JetStream. JetStream adds a
    persistence write per message and will be measurably slower; measure P2
    before committing to it for high-rate paths.
- At the time of this P0 measurement the NATS C client (`nats.c`) was not yet
    integrated; the numbers above come from the Go-based `nats bench` CLI,
    which has its own serialisation overhead. The P1 C-native integration
    (`nats.c`, now the library's sole transport) has since shipped; see
    `native/benchmarks/bench_stream.c` for current end-to-end NATS numbers
    over the real C client rather than the CLI proxy.

### 9.2 Status / control plane (PUB/SUB + REQ/REP)

The status plane carries small control / telemetry messages at a low rate
(Hz–kHz), so **throughput is irrelevant** — its figures of merit are *unloaded*
latency and **delivery semantics**. Latency is a non-issue either way:

| pattern              | msg  | RTT min | RTT mean | RTT p99 | RTT max | one-way (≈) |
| -------------------- | ---- | ------- | -------- | ------- | ------- | ----------- |
| ZMQ REQ/REP (idle)   | 64 B | 14 µs   | 30 µs    | 51 µs   | 282 µs  | ~15 µs      |
| ZMQ REQ/REP (loaded) | 64 B | 224 µs  | 375 µs   | 998 µs  | —       | ~110–190 µs |

(Both rows are loopback, 20k pings. The **idle** row is the real figure; the
**loaded** row was taken with concurrent builds running — the firehose sweep was
~4–5× slower than §9.1 at the time — and stands as a worst-case ceiling.) The
takeaway is the order of magnitude: even loaded, a sub-millisecond round trip is
**negligible against status update periods**, so latency does not drive the
choice. **Semantics do** — and this is the axis NATS exists for:

| property                      | ZMQ PUB/SUB                                                                   | NATS                                                                             |
| ----------------------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| late joiner / "current state" | **lost** — slow-joiner drops msgs sent before the SUB connects; no last value | **JetStream KV / last-value** — a new subscriber reads current state immediately |
| delivery                      | fire-and-forget (lossy on a slow consumer)                                    | at-least-once + acks + redelivery (JetStream)                                    |
| fan-out / filtering           | N subscribers; topic conventions by hand                                      | subjects + wildcards; broker-side filtering                                      |
| discovery / reconnect         | endpoints hard-wired; no topology reconnect                                   | built-in reconnect, service discovery, clustering                                |
| request / reply               | strict lock-step; head-of-line blocking                                       | request-reply with timeouts + queue groups, no HOL                               |

The **late-joiner / last-value** gap is usually decisive for a control plane:
ZMQ PUB/SUB cannot answer "what is the current state?" for a subscriber that
joined after the last update, while NATS does so natively. So the status plane
is a NATS win on semantics, with latency a non-factor.

### 9.3 Interpretation for the phased plan

- **Firehose → ZMQ, co-located.** At DSP block sizes (4k–64k samples) ZMQ
    delivers 1.85–2.3× a NATS broker's throughput on loopback/ipc — but that
    edge is a co-located phenomenon (on a real NIC both are link-bound; see the
    headline caveat). Brokerless socket-to-socket is the right firehose.
- **Status/control → NATS, distributed.** Throughput is moot; NATS wins on
    last-value/late-joiner, durable+acked delivery, reconnect, and discovery —
    exactly the resilient/k8s axis.
- **The routing boundary is the *plane*, not just the frame size.** High-rate
    co-located I/Q → ZMQ; resilient/distributed status + control → NATS. (For a
    firehose that must cross hosts resiliently, NATS is viable above ~1k-sample
    frames where its throughput keeps up and the NIC is the real limit anyway.)
- **P1 greenlit:** the numbers confirm the pluggable-transport seam is the right
    architecture — ZMQ for the co-located firehose, NATS for the distributed
    status/control plane, behind one interface.
