- **A captured burst's event carries its Doppler rate.** `events()` gains
    `doppler_rate_hz_s` (and `BurstCapture.doppler_rate_hz_s` the last
    event's): refine's winning hypothesis from the whole preamble, the grid
    point nearest the carrier's rate in every event 23 dB over the design
    point (#2219). `burst_capture_event_t` grows a field, so C consumers
    rebuild; the state blob goes to version 6, and a blob saved by v0.71.0 or
    earlier is refused, so restore it with the release that wrote it.
