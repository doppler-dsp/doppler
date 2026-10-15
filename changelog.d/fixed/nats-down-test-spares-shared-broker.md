- **The `nats-down` test no longer purges the broker the rest of the suite
    is using.** It ran the real recipe, whose first step deletes every
    `DP_WORK_*` stream, and under xdist that removed other workers' work
    queues mid-test: a Pull read stopped short 5 times in 200 CI runs.
    `scripts/nats_streams.py` takes `--port` (or `NATS_STREAMS_PORT`), and
    the test aims the purge and `docker` away from shared state (#2319).
