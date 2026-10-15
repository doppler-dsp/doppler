- **`BurstCapture`'s state blob is version 6, and a blob saved by v0.71.0 or
    earlier is refused** (`set_state()` raises `ValueError`), as is any blob
    that embeds one. Restore it with the release that wrote it, or re-create
    the object. In C, `burst_capture_event_t` gains `doppler_rate_hz_s`, so
    rebuild everything that includes it (#2219).
