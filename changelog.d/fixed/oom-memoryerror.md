- **An allocation failure raises `MemoryError`, not `ValueError`**
    (#2846). `Acquisition.set_carrier_freq_hz` and `Acquisition.set_state`
    (a carrier change rebuilds the delay-Doppler map), `BurstCapture.set_state`
    and `AsyncDsssReceiver.set_telemetry`. `set_telemetry`'s C core now
    returns `DP_ERR_MEMORY` when the attach cannot allocate, its scratch
    context or a loop's own, where it returned `DP_ERR_INVALID`. A refused
    argument or blob still raises the same `ValueError`.
