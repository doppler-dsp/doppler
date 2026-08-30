- **Leftover JetStream work queues are now purged, and counted.** Every
    `Push(endpoint)` provisions a durable `DP_WORK_<endpoint>` stream and
    nothing removed one; `nats-down` only cleaned a broker this repo started,
    while `start-nats.sh` reuses one already on 4222. One dev box had reached
    **5,236 streams / 40.8 GiB**, and because a work queue is keyed by a
    repeating endpoint, a run opened onto the previous era's queue — 174,133
    unreadable frames — failing a compose test there while CI stayed green.
    `nats-down` now purges first, `make nats-purge` is the remedy, and
    `nats-up` fails above 500 — about 25 suite runs, at the measured 20 leaked
    per run — rather than letting it reach four figures unseen.
