- **A work queue no longer grows without bound** — it was created file-backed
    with `jsStreamConfig_Init` defaults (no MaxAge/MaxBytes/MaxMsgs) and nothing
    ever deleted it; a work queue drops a frame only when a consumer *acks* it,
    so a producer with no consumer was an unbounded disk sink — **40 GB** of
    residue from repeated test runs ([#1136][gh1136]). Auto-created queues now
    carry a one-hour age bound (pre-provision the stream to choose your own),
    and `Push.delete_stream()` lets a caller that owns the queue's lifetime end
    it — never automatic, since outliving one producer is the point. A failed
    send now also names the transport's error instead of a bare `"Send error"`.

[gh1136]: https://github.com/doppler-dsp/doppler/issues/1136
