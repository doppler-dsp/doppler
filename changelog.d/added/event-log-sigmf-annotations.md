- **`telemetry.EventLog` — a run's events as SigMF annotations.** A transition
    (seeded, tracking, lost, a stream gap) is a span of the sample stream, so
    it is an annotation, appended live to a tail-able JSON-Lines file and
    finalized into a `.sigmf-meta` through the writer's existing emitter. The
    reader now parses SigMF's `core:datetime` too, so a SigMF replay lands on
    its own timeline. Design:
    [§8.1](https://github.com/doppler-dsp/doppler/blob/main/docs/design/async-dsss-receiver.md).
