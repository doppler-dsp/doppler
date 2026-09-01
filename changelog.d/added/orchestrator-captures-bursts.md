- **The acquisition bank captures bursts, not just detections.**
    `Acquirer(..., burst_len=N, ring_dir=...)` makes every channel a
    `DDC → BurstCapture`; `process()` still returns the detections and
    `bursts()` returns each channel's aligned windows. A file-backed bank
    resumes across a pod restart over the same ring directory.
    [#1174](https://github.com/doppler-dsp/doppler/issues/1174),
    [#1180](https://github.com/doppler-dsp/doppler/pull/1180).
