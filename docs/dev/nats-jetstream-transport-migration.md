# Migrate ZMQ transport → resilient NATS JetStream I/Q streaming (k8s + HPA)

## Context

doppler's live I/Q transport (`native/src/stream/stream_core.c`, public API
`native/inc/stream/stream.h`) is a thin layer over **vendored static libzmq**,
exposing three ZMQ patterns — PUB/SUB (fan-out), PUSH/PULL (pipeline), REQ/REP
(control) — all framed with the 96-byte `dp_header_t` wire header (magic
`0x53494753` "SIGS", per-sender `sequence`, `timestamp_ns`, `sample_rate`,
`center_freq`, `num_samples`). It works, but ZMQ gives us no broker-side
durability, no replay, no decentralized resilience, and no clean k8s autoscaling
story. There is currently **no NATS, no Kubernetes, and no Helm** anywhere in
the repo (only a demo `Dockerfile` + `docker-compose.yml`).

Goal: add a **NATS / JetStream** transport that is fault-tolerant, decentralized
(no single-broker SPOF), and horizontally autoscalable under Kubernetes + HPA,
**without breaking the existing ZMQ users** and **without compromising doppler's
C-first, `-lm`-only-core invariants**.

### Decisions locked in (from user)

1. **Tiered transport** — Core NATS for the hot live-I/Q fan-out path
    (at-most-once, in-memory, line-rate); JetStream only for durable/balanced
    work-queue, control, and record/replay.
1. **Coexist (pluggable backend)** — keep ZMQ; select backend by endpoint
    scheme. `nats://…` → NATS; `tcp://` / `ipc://` → ZMQ. One `stream.h`, two
    backends, chosen at create time. Deprecate ZMQ later, not now.
1. **Topology** — user is flexible; we adopt the idiomatic streaming-DSP answer:
    a **nats-server sidecar per pod/node** (clients connect to `127.0.0.1:4222`),
    servers clustered into a **JetStream RAFT mesh** + optional leaf nodes for
    edge peers. No central broker SPOF; the C library stays a pure client (it
    *cannot* embed the Go nats-server — that's a deployment concern, not C code).
1. **Autoscaling** — **KEDA** NATS-JetStream scaler (scales consumer
    deployments on pending/unacked message lag, scale-to-zero capable) with
    CPU/memory **HPA** as a secondary trigger.

### Transport tier mapping

| Current ZMQ          | New NATS tier                                            |
| -------------------- | -------------------------------------------------------- |
| PUB/SUB (fan-out)    | **Core NATS** publish/subscribe on subjects (lossy-OK)   |
| PUSH/PULL (pipeline) | **JetStream** work-queue stream + durable pull consumers |
| REQ/REP (control)    | **NATS native request/reply** (`natsConnection_Request`) |
| (none — new)         | **JetStream** file-storage stream for record/replay      |

______________________________________________________________________

## Phase 0 — Backend-abstraction refactor (no behavior change)

Make the existing code dispatchable before adding NATS. Pure refactor; existing
C tests + `pytest src/doppler/stream/` must stay green.

In **`native/src/stream/stream_core.c`**:

- Convert `struct dp_ctx` to a **tagged union** (NOT a vtable — only two
    backends, thin-glue ethos):
    ```c
    typedef enum { DP_BACKEND_ZMQ = 0, DP_BACKEND_NATS = 1 } dp_backend_t;
    struct dp_ctx {
      dp_backend_t backend;
      dp_sample_type_t sample_type;
      uint64_t sequence;            /* shared, backend-agnostic */
      union { struct dp_zmq_state zmq; struct dp_nats_state nats; } u;
    };
    ```
- Convert `struct dp_msg` to a tagged union too (`zmq_msg_t` by value vs
    `natsMsg *` owned pointer), because zero-copy lifetime differs.
- Lift the current ZMQ bodies into `zmq_*` static helpers. Add a one-line
    backend branch inside the ~6 shared funnels: `ctx_create`, `send_signal`,
    `recv_signal`, `recv_raw`, `set_recv_timeout`, and `dp_msg_data/free`.
- Add scheme parse: `strncmp(ep, "nats://", 7)` → NATS, else ZMQ. Change the six
    public `*_create` fns to pass a **role enum** (decoupled from `ZMQ_PUB` etc.);
    `ctx_create` resolves role→ZMQ-type only on the ZMQ branch.

**Every public `dp_pub_send_*` / `dp_*_create` signature stays identical** — they
still call the shared funnels. `stream.h` gets no API change in this phase.

______________________________________________________________________

## Phase 1 — Vendor nats.c (build integration)

Mirror the existing libzmq vendoring exactly (`CMakeLists.txt:~295-365`):

- Add `vendor/nats.c/`. An `add_custom_command` builds it static+PIC into
    `build/libnats-vendor/` with `NATS_BUILD_STATIC_LIB=ON`,
    `BUILD_SHARED_LIBS=OFF`, **`NATS_BUILD_WITH_TLS=OFF`** for v1 (avoids an
    OpenSSL vendoring project; TLS is a later opt-in), nkeys/libsodium off.
- Expose an `IMPORTED` `nats_vendor_static` target; link it into the stream
    objects and fold `libnats.a` into `doppler_stream_static` via a **second**
    `merge_static_libs.cmake` call (`cmake/merge_static_libs.cmake` — extend to
    accept a list, or invoke twice).
- **Verify at the pinned tag** whether protobuf-c (JetStream) is in-tree
    (`deps/`/`src/`, expected) or external — if in-tree, JetStream adds zero
    external deps. nats.c is **C, not C++**, so it adds *no* new C++ symbols.

**Invariant preserved:** the pure-C core `libdoppler.so`/`.a` never references
stream/nats objects, so the CI `nm` C++-free gate (`ci.yml:~84`) and the
`-lm`-only static-link smoke test stay green untouched. NATS lands entirely
inside the already-quarantined `libdoppler_stream` tier.

Update **`Dockerfile`**: drop `libzmq3-dev`/`libzmq5` reliance toward the
vendored-static model; no NATS system package needed (client is static).

______________________________________________________________________

## Phase 2 — NATS Core PUB/SUB + REQ/REP (`native/src/stream/stream_nats.c`)

New TU holding all nats.c-specific logic; `stream_core.c` stays the dispatcher.

- **Connect / failover:** `natsOptions_SetServers` (localhost + optional seed
    URLs for mesh failover), auto-reconnect (`SetMaxReconnect`,
    `SetReconnectBufSize`), **synchronous** subscriptions only
    (`natsSubscription_NextMsg`) so the existing GIL-release-around-blocking-recv
    pattern in `stream_ext.c` transfers unchanged.
- **Subject scheme:** endpoint `nats://host:4222/<base>` → base subject
    (default `iq`). Publish to `iq.<stream_id>.<sample_type>`; subscribe to
    `iq.<base>.>` (preserves ZMQ "subscribe to all"). Putting `sample_type` in the
    subject enables broker-side type filtering — a free upgrade over ZMQ.
- **Message format:** keep `dp_header_t` as a **96-byte binary prefix** in the
    single NATS payload (`[header][I/Q]`), NOT NATS string headers — preserves the
    cross-language byte-identical wire invariant and the zero-copy recv contract
    (`dp_msg_data` points into the `natsMsg` buffer; `dp_msg_free` →
    `natsMsg_Destroy` exactly once).
- **REQ/REP:** requester uses `natsConnection_Request`; replier stores the
    inbound `natsMsg_GetReply` subject (`last_reply_subj`) so `dp_rep_send` answers
    it — matching ZMQ's "recv-before-send" REP discipline.

______________________________________________________________________

## Phase 3 — Chunking for >1 MB I/Q blocks (mandatory)

NATS default `max_payload` is 1 MiB; ZMQ has no such limit. A single 64K-sample
CF64 block (1 MiB) already exceeds it — a naive port silently fails to publish.

- Keep `dp_header_t` **96 bytes** (wire-compatible). Use `flags` +
    `reserved[]`: `DP_FLAG_CHUNKED`, `reserved[0]=chunk_index`,
    `reserved[1]=chunk_count`, `reserved[2]=total_num_samples`. Same `sequence`
    across all chunks of one logical frame; `(sequence, chunk_index)` orders
    reassembly.
- Auto-size the chunk limit from `natsConnection_GetMaxPayload()` after connect
    (robust if an operator raises the server limit) — don't hardcode 1 MB.
- Receiver reassembles into one doppler-owned buffer (a third `dp_msg` union
    variant: `malloc`'d, freed in `dp_msg_free`) so the caller API is unchanged.
    Small blocks (the common live case) stay single-message **and zero-copy**.

______________________________________________________________________

## Phase 4 — JetStream PUSH/PULL + replay tier

- **PUSH** (`dp_push_create` on `nats://`): get `jsCtx`, **lazily + idempotently**
    create a work-queue stream `DP_WORK_<base>` (`subjects=["work.<base>.>"]`,
    `retention=WorkQueue`). Storage: **memory** for PUSH/PULL (ZMQ-ephemeral
    parity); swallow `AlreadyInUse`; return clear `DP_ERR_INIT` if the server has
    no `-js`.
- **PULL** (`dp_pull_create`): durable pull consumer `DP_PULL_<base>` shared
    across workers (JetStream load-balances → round-robin analog with redelivery),
    `ack_policy=Explicit`, `max_ack_pending=1000` (the ZMQ HWM=1000 backpressure
    analog). `dp_pull_recv` = `js_Fetch(batch=1, timeout)`; **ack-at-recv** for v1
    (API-identical, at-most-once on crash — matching ZMQ). A future `dp_msg_ack`
    could add true at-least-once additively.
- **Replay tier:** same mechanism with `retention=Limits` +
    `storage=File`; document replay-from-sequence/time in the gallery.

______________________________________________________________________

## Phase 5 — Python + Rust + tests

- **Python:** no binding-code change expected — sync recv + GIL release transfer,
    `dp_msg_free` branch is internal. The `Publisher/Subscriber/Push/Pull/...`
    classes (`native/src/stream/stream_ext.c`) and `doppler.stream` work as-is with
    `nats://` endpoints. Add integration tests in
    `src/doppler/stream/tests/test_stream.py` parametrized over `tcp://` vs
    `nats://`.
- **C tests:** `native/tests/test_stream_nats_core.c` — connect, pub/sub
    round-trip, chunked >1 MB block reassembly, JetStream work-queue
    load-balance + ack, `natsMsg` lifetime (no leak/double-free). These need a live
    broker (unlike the in-process ZMQ tests).
- **Rust ffi** has no streaming bindings today — out of scope (note only).
- **`wfm_sink`** (`native/src/wfm/wfm_sink.c`): add a `nats://` path so
    `ZmqSink` (or a renamed `StreamSink`) can publish over NATS via the same
    backend — reuses the weak-stub/strong-def split already in place.

______________________________________________________________________

## Phase 6 — Kubernetes + HPA/KEDA + CI

New `deploy/` tree (no existing k8s anything):

- **`deploy/helm/nats/`** — NATS via the official chart values: JetStream
    enabled, 3-node cluster (RAFT), file storage PVCs, **sidecar/DaemonSet** model
    so each node has a local server; clients connect to `127.0.0.1:4222`.
- **`deploy/helm/doppler-pipeline/`** — Deployments for producers + consumers
    (consumers as the autoscaled tier), `STREAM_ENDPOINT=nats://127.0.0.1:4222/...`.
- **`deploy/keda/scaledobject.yaml`** — KEDA `nats-jetstream` trigger scaling the
    consumer Deployment on pending/unacked lag (scale-to-zero), **plus** a CPU/mem
    HPA-style fallback trigger. (KEDA generates the HPA under the hood.)
- **`deploy/docker/`** — production multi-stage images (replace the demo
    `docker-compose.yml` services with NATS-based equivalents; keep compose for
    local dev with a `nats -js` container).
- **CI** (`.github/workflows/ci.yml`): add a `nats-server -js` **service
    container** to the stream test job (chunking + JetStream tests need a live
    broker). Re-run `jm status --check` / `nm` gates unchanged.

______________________________________________________________________

## Key risks (designed-for)

- **>1 MB blocks** → Phase 3 chunking is mandatory, not optional.
- **nats.c is client-only** → "embedded per node" is a sidecar deployment, not C.
- **Loss/ordering differ per tier** → Core NATS drops on slow consumer (like ZMQ
    HWM) but we keep `dp_header_t.sequence` so subscribers detect gaps; JetStream
    is stronger (acked, redelivered).
- **Zero-copy send is lost** on NATS (header‖data concat); recv stays zero-copy
    for unchunked. Note in benchmarks.
- **`natsMsg` ownership** (one `natsMsg_Destroy` in `dp_msg_free`; don't double
    free the `js_Fetch` list) → focused C lifetime test.
- **TLS off in v1** → plaintext localhost-sidecar; add OpenSSL-backed TLS +
    nkey/creds auth later (additive).

## Open decisions (flag, non-blocking)

- PUSH/PULL JetStream storage: **memory** (ZMQ parity, recommended) vs file.
- Ack timing: **ack-at-recv** (API-identical, recommended) vs add `dp_msg_ack`.
- TLS/auth in v1: **off** (recommended) vs on.

______________________________________________________________________

## Critical files

- `native/src/stream/stream_core.c` — tagged-union refactor + backend dispatch
- `native/src/stream/stream_nats.c` — **new**, all nats.c logic
- `native/inc/stream/stream.h` — no API change; add chunk-flag macros, document
    `nats://` scheme, keep `dp_header_t` 96 bytes
- `CMakeLists.txt` + `native/src/stream/CMakeLists.txt` — vendor nats.c static,
    link, fold into `doppler_stream_static`
- `cmake/merge_static_libs.cmake` — fold a second static archive
- `native/src/stream/stream_ext.c` + `src/doppler/stream/tests/test_stream.py` —
    Python integration tests over `tcp://` vs `nats://`
- `native/src/wfm/wfm_sink.c` — `nats://` sink path
- `native/tests/test_stream_nats_core.c` — **new** C tests
- `deploy/helm/**`, `deploy/keda/**`, `deploy/docker/**` — **new** k8s/autoscale
- `.github/workflows/ci.yml` — `nats-server -js` service for stream tests
- `Dockerfile` / `docker-compose.yml` — NATS-based images + local dev

## Verification

1. **Refactor regression (Phase 0):** `ctest --test-dir build -R stream` and
    `pytest src/doppler/stream/` green with zero behavior change on `tcp://`.
1. **Build invariants (Phase 1):** `nm -CD build/libdoppler.so | grep -E 'GLIBCXX_|CXXABI_|std::'` empty; static-consumer smoke links `libdoppler.a -lm` only.
1. **Round-trip (Phase 2-4):** run a local `nats-server -js`; C transmitter on
    `nats://127.0.0.1:4222/iq` → Python `Subscriber` receives byte-identical
    samples + header; verify `sequence` gap detection; verify a >1 MB CF64 block
    reassembles correctly (chunking); verify two PULL workers load-balance a
    JetStream work-queue and ack.
1. **Cross-language:** C publisher ↔ Python subscriber and vice versa, proving
    the `dp_header_t` binary-prefix wire is byte-identical across backends.
1. **k8s/autoscale:** deploy Helm charts to a kind/minikube cluster with KEDA;
    drive load and confirm the consumer Deployment scales up on JetStream lag and
    back to zero when idle; kill a NATS pod and confirm clients fail over (no data
    loss on JetStream tier).
