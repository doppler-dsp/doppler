# doppler streaming — resilient NATS tier on Kubernetes

This tree deploys doppler's **resilient, autoscaling I/Q transport** on
Kubernetes: a NATS JetStream work-queue that survives pod crashes, scales
consumers on backlog, and never drops a frame. It complements — does not
replace — the existing ZMQ transport.

- **Design:** `docs/dev/nats-jetstream-transport-migration.md`
- **Benchmarks / rationale:** `docs/design/streaming-roadmap.md`
- **C/Python API:** `native/inc/stream/stream.h`, `doppler.stream`

______________________________________________________________________

## 1. Why two transports

doppler routes by **plane**, not by frame size:

| Plane                                | Transport                     | Endpoint            | Why                                                           |
| ------------------------------------ | ----------------------------- | ------------------- | ------------------------------------------------------------- |
| **Co-located I/Q firehose**          | ZMQ (unchanged)               | `tcp://` / `ipc://` | Brokerless, lowest latency, link-bound on a real NIC anyway   |
| **Distributed / resilient firehose** | **NATS JetStream** work-queue | `nats://…`          | Durable, at-least-once, autoscalable — what this tree deploys |
| **Status / control / telemetry**     | NATS Core pub/sub + req/reply | `nats://…`          | Last-value, late-joiner, reconnect, discovery                 |

The same 96-byte `dp_header_t` "SIGS" wire envelope is used on both backends, so
a ZMQ producer and a NATS consumer agree byte-for-byte. The C library is a pure
NATS **client** (vendored `nats.c`); the Go `nats-server` is infrastructure
(sidecar / StatefulSet), never embedded in the library.

### Selecting a backend

The endpoint scheme picks the backend at create time — no API change:

```c
dp_push_t *p = dp_push_create("nats://nats:4222/myfeed", CF32); /* JetStream */
dp_push_t *z = dp_push_create("tcp://*:5555",            CF32); /* ZMQ       */
```

`nats://host:port/<base>` → subjects `work.<base>.>` (work-queue) and
`iq.<base>.>` (pub/sub); `<base>` defaults to `default`.

### At-least-once: ack after you process

A `nats://` Pull consumer must acknowledge each frame once processed; an
un-acked frame is **redelivered** if the worker dies (this is the zero-loss
guarantee). `dp_msg_ack` is a no-op on ZMQ / NATS-core, so callers can ack
unconditionally.

```c
dp_msg_t *m; dp_header_t h;
dp_pull_recv(pull, &m, &h);
/* ... process dp_msg_data(m) ... */
dp_msg_ack(m);          /* Python: pull.ack(samples) */
dp_msg_free(m);
```

______________________________________________________________________

## 2. Layout

| Path                              | What                                                             |
| --------------------------------- | ---------------------------------------------------------------- |
| `docker/stream_tool.c`            | Reference producer/consumer (PN self-verifying I/Q).             |
| `docker/Dockerfile`               | Multi-stage build of `stream_tool` (static C++ runtime).         |
| `nats/values.yaml`                | Official NATS chart values — 3-node R=3 JetStream, file storage. |
| `pipeline/`                       | Helm chart: producer + (autoscaled) consumer Deployments.        |
| `keda/consumer-scaledobject.yaml` | KEDA ScaledObject — scale on consumer lag, 2 → 50.               |
| `docker-compose.nats.yml`         | Single-node JetStream broker for local dev / tests.              |

______________________________________________________________________

## 3. Local development (no cluster)

Run a single-node JetStream broker and exercise the `nats://` path:

```sh
docker compose -f deploy/docker-compose.nats.yml up -d
pytest src/doppler/stream/          # the nats:// tests run instead of skipping
docker compose -f deploy/docker-compose.nats.yml down
```

Single node ⇒ no R=3; this proves protocol + at-least-once redelivery, not
broker-pod-death failover (that needs the 3-node cluster, §5).

______________________________________________________________________

## 4. Build the image

`stream_tool` links the doppler core (`libdoppler.a`, the `lo`/`pn` objects)
and the stream tier (`libdoppler_stream.a`, vendored zmq+nats folded in). The
C++ runtime is linked **statically** (`-static-libstdc++`), so the runtime
image needs only libc.

```sh
# Host docker (needs docker-buildx for BuildKit):
docker build -t doppler-stream:dev -f deploy/docker/Dockerfile .

# For minikube, load it into the cluster's daemon:
minikube image load doppler-stream:dev
# …or build straight into minikube (no host buildx needed):
minikube image build -t doppler-stream:dev -f deploy/docker/Dockerfile .
```

The root `.dockerignore` keeps the host `build/` out of the context (otherwise
the in-container `cmake -B build` collides with the host's CMake cache).

______________________________________________________________________

## 5. Deploy on Kubernetes

```sh
# 1. NATS JetStream cluster (3 nodes, RAFT, file storage)
helm repo add nats https://nats-io.github.io/k8s/helm/charts/
helm upgrade --install nats nats/nats -n doppler --create-namespace \
  -f deploy/nats/values.yaml
kubectl rollout status statefulset/nats -n doppler

# 2. Pre-provision the R=3 work-queue stream (the client's idempotent create
#    then attaches to it instead of making an R=1 one). The `nats` CLI lives in
#    the chart's nats-box pod:
BOX=$(kubectl get pod -n doppler -l app.kubernetes.io/component=nats-box \
  -o name | head -1)   # or: kubectl get pods -n doppler | grep nats-box
kubectl exec -n doppler "$BOX" -- nats --server nats://nats:4222 \
  stream add DP_WORK_default --subjects 'work.default.>' \
  --storage file --replicas 3 --retention work --discard new --defaults

# 3. Producer + consumer Deployments
helm upgrade --install pipeline deploy/pipeline -n doppler \
  --set image.repository=doppler-stream --set image.tag=dev \
  --set image.pullPolicy=Never \
  --set 'consumer.resources.requests.cpu=25m' \
  --set 'consumer.args[0]=consume' --set 'producer.args[0]=produce'

# 4. KEDA + the autoscaler
helm repo add kedacore https://kedacore.github.io/charts
helm upgrade --install keda kedacore/keda -n keda --create-namespace
kubectl apply -f deploy/keda/consumer-scaledobject.yaml
```

______________________________________________________________________

## 6. What it guarantees, and how it's verified

This stack was verified end-to-end on minikube (3-node R=3 JetStream + KEDA).

### Zero loss on a **broker** pod death (RAFT quorum)

```sh
# produce N sync-acked frames (persisted to the R=3 quorum), then:
kubectl delete pod nats-1 -n doppler          # kill the stream leader
# RAFT re-elects a new leader; `nats stream info DP_WORK_default` still shows
# Messages: N. Drain afterwards → all N consumed. Nothing lost.
```

Producers use synchronous, server-acked `js_Publish` — a frame is persisted
(and replicated) before `send` returns.

### Zero loss on a **consumer** pod death (redelivery)

A consumer that dies with un-acked frames has them redelivered (after
`AckWait`, 5 s) to another worker on the shared durable consumer. With
`minReplicaCount: 2` a live peer is always ready, so failover is instant —
zero loss *and* minimal recovery latency. Durability is **not** a function of
replica count; it comes from the durable R=3 file stream + explicit ack.

### Autoscaling on lag (KEDA "eats HPA")

KEDA's `nats-jetstream` scaler reads the consumer's pending/un-acked count and
drives an HPA off **that** (not CPU). Under load the consumer Deployment scales
**2 → 50**; when the backlog drains it scales back to 2.

```sh
kubectl get hpa,deploy/doppler-consumer -n doppler -w   # watch it climb on lag
```

### No dropped/corrupted bits (PN self-verification)

`stream_tool`'s producer fills each frame with an MLS/PN sequence (doppler `pn`
object) seeded by the frame index, which it assigns as the wire
`header.sequence`. The consumer regenerates the PN from that sequence and
bit-compares — so a dropped, duplicated, or corrupted frame is *caught*, not
just counted. Under the 2→50 scale test, all 50 consumers reported
`0 PN mismatch`.

______________________________________________________________________

## 7. Troubleshooting

| Symptom                                                               | Cause / fix                                                                                                                                                                              |
| --------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| KEDA HPA target `<unknown>`                                           | The scaler reads NATS monitoring on **:8222**, which the chart exposes on the **headless** service. Point `natsServerMonitoringEndpoint` at `nats-headless.<ns>.svc.cluster.local:8222`. |
| ScaledObject rejected: "cpu trigger but container has no cpu request" | Give the consumer a CPU request (`consumer.resources.requests.cpu`).                                                                                                                     |
| Pods `ErrImageNeverPull`                                              | The image isn't in the node's container runtime. `minikube image load doppler-stream:dev` (or build with `minikube image build`), and use `image.pullPolicy: Never`.                     |
| `BuildKit is enabled but buildx is missing`                           | Install `docker-buildx`, or build with `minikube image build` (uses the in-cluster builder).                                                                                             |
| In-container `cmake` cache errors                                     | The host `build/` leaked into the context — ensure the root `.dockerignore` excludes it.                                                                                                 |
| Stream created at R=1 in prod                                         | Pre-provision it at R=3 (step 5.2) before the client connects; the client's create tolerates an existing stream.                                                                         |

______________________________________________________________________

## 8. Teardown

```sh
helm uninstall pipeline nats -n doppler
helm uninstall keda -n keda
kubectl delete -f deploy/keda/consumer-scaledobject.yaml
minikube delete            # nuke the whole local cluster
```
