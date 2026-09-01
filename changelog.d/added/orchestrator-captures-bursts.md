- **The acquisition bank captures bursts, not just detections.**
    `Acquirer(..., burst_len=N)` makes every `CoarseChannel` a
    `DDC → BurstCapture`, so one pass over the stream yields both the
    dedup'd `Detection` records (`process()`) and each channel's aligned
    burst windows (`bursts()`, indexed by `Detection.channel`).
    `ring_dir=` backs every channel's look-back with a file named by its
    centre frequency, which is what lets a bank checkpointed on one pod
    resume across a restart on another over the same volume — the HPA
    case. `BurstCapture.detections()` is what makes one object enough:
    without it a bank wanting both faces would push every sample through
    two acquisition engines. `BurstCapture.doppler_res_hz` now reads the
    engine's bin width from construction rather than the last event's
    mirror of it (which read `0.0` before any burst and collapsed the
    bank's cross-channel dedup to exact equality).
    Refs [#1174](https://github.com/doppler-dsp/doppler/issues/1174).
