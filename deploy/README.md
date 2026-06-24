# Deploying the doppler NATS streaming tier on Kubernetes

The resilient transport tier (see `docs/dev/nats-jetstream-transport-migration.md`
and `docs/design/streaming-roadmap.md`) routes by **plane**:

- **co-located I/Q firehose → ZMQ** (`tcp://`/`ipc://`) — unchanged, fastest.
- **distributed/resilient + status → NATS** (`nats://`) — what this tree
    deploys.

The doppler C library is a pure NATS *client* (vendored `nats.c`); the Go
`nats-server` runs as infrastructure, never embedded in the library.

## Pieces

| Path                              | What                                                                                                                                    |
| --------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `nats/values.yaml`                | Values for the official NATS Helm chart — 3-node JetStream cluster (RAFT), file storage. Survives a broker-pod death with no data loss. |
| `pipeline/`                       | Helm chart: a producer Deployment (PUSH) + a consumer Deployment (PULL+ack).                                                            |
| `keda/consumer-scaledobject.yaml` | KEDA `nats-jetstream` ScaledObject — scales consumers **2 → 50** on consumer lag (+ CPU as a secondary trigger).                        |
| `docker-compose.nats.yml`         | Single-node JetStream broker for local dev / running the `nats://` tests.                                                               |

## Install

```sh
# 1. NATS JetStream cluster (R=3, file storage)
helm repo add nats https://nats-io.github.io/k8s/helm/charts/
helm upgrade --install nats nats/nats -n doppler --create-namespace \
  -f deploy/nats/values.yaml

# 2. Pre-provision the R=3 work-queue stream (the client's create then attaches
#    to it instead of making an R=1 one):
nats stream add DP_WORK_default --subjects 'work.default.>' \
  --storage file --replicas 3 --retention work --discard new --defaults

# 3. The producer + consumer Deployments
helm upgrade --install pipeline deploy/pipeline -n doppler \
  --set image.repository=ghcr.io/doppler-dsp/doppler

# 4. KEDA + the autoscaler
helm repo add kedacore https://kedacore.github.io/charts
helm upgrade --install keda kedacore/keda -n keda --create-namespace
kubectl apply -f deploy/keda/consumer-scaledobject.yaml
```

## Why it doesn't drop a bit

Durability comes from the **durable, R=3, file-backed work-queue stream +
explicit ack**, not from the consumer replica count:

- A consumer can crash mid-frame: its un-acked messages redeliver (`AckWait`)
    to another worker. The `minReplicaCount: 2` floor means a live peer is always
    ready, so failover is instant rather than waiting on a cold start.
- A broker pod can die: the RAFT quorum (R=3) keeps the stream and its
    messages; clients auto-reconnect (the C client sets `AllowReconnect`).
- Producers use synchronous, server-acked `js_Publish` — a frame is persisted
    (and replicated) before `send` returns.

## Verifying on a cluster (deploy-time)

YAML/templating is checked in CI; the chaos + scale behaviour needs a real
cluster (kind/minikube):

```sh
# scale: drive load, watch consumers climb toward 50 on lag, settle to 2 idle
kubectl get scaledobject,hpa,deploy -n doppler -w

# broker-pod death: kill the JetStream leader, confirm zero loss
kubectl delete pod nats-0 -n doppler          # RAFT re-elects; no data lost

# consumer churn: repeatedly kill consumer pods under load, then assert the
# received sequence set == the sent set (no gaps).
```

For a laptop smoke test of protocol + at-least-once redelivery (no R=3):

```sh
docker compose -f deploy/docker-compose.nats.yml up -d
pytest src/doppler/stream/      # the nats:// tests run instead of skipping
```
