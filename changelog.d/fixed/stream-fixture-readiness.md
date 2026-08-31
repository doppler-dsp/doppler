- **The stream tests wait for readiness instead of guessing at it** — the
    push/pull, pub/sub and req/rep fixtures replaced a fixed `time.sleep` with a
    probe that proves the endpoint actually carries a frame, which is what the
    sleep was standing in for ([#1131][gh1131]). Three helpers, because the
    patterns differ: a work queue persists so the probe is sent once, core NATS
    drops a publish with no subscriber so it is repeated and drained, and
    request/reply has to complete a round trip because a delivered-but-unanswered
    request leaves both ends mid-exchange. The seven remaining sleeps each state
    why they are not readiness waits.

[gh1131]: https://github.com/doppler-dsp/doppler/issues/1131
