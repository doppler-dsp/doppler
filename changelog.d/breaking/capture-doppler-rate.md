- **The burst captures take the Doppler rate, and the receiver passes its
    own** (#1490). `BurstCapture` and `PersistentBurstCapture` take
    `doppler_rate` (Hz/s, default 0 = no bound), as `BurstAcquisition` does.
    `DsssBurstReceiver` passes its `max_rate` (cycles/sample², the rate its
    demod already searches) to acquisition as `max_rate · fs²`. With
    `max_rate > 0` it can now size a shallower search. **Breaking for C:**
    `burst_capture_create()` and `burst_capture_create_backed()` gain a
    trailing `double doppler_rate`; append `0.0` to keep today's depth.
