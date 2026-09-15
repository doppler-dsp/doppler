- **A `Pull` worker started before any `Push` died on a fresh broker**
    ([#956](https://github.com/doppler-dsp/doppler/issues/956)). Only the
    producer created the JetStream work-queue stream, so `dp_pull_create`
    failed against a broker that had never carried it, and the start order
    of a sender and its workers mattered. The worker now provisions the stream
    through the same idempotent helper the producer uses, so either side may
    start first and a pre-provisioned stream is still adopted as-is.
