# Streaming roadmap: resilient transport for Kubernetes

**Status:** draft / for discussion
**Scope:** doppler's network streaming of I/Q (and metadata) — where it goes
beyond today's ZeroMQ transport layer.

______________________________________________________________________

## 1. Context

doppler already ships **three ZMQ messaging patterns** via `libdoppler_stream`
(opt-in, vendored static libzmq — see `EXTENSION_WITH_STATIC_ZMQ.md`):

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
