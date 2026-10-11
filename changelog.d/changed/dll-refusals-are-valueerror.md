- **A refused create raises `ValueError`, not `MemoryError`, and a coupled
    `carrier_freq_hz` must be 0 or above half the sample rate.** The
    receivers, `Dll`, `Despreader` and the loops name the argument they
    refuse; `Acquisition.set_carrier_freq_hz` refuses the same band (#2103).
