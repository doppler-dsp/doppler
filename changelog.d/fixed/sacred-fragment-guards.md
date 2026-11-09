- **`Telemetry`, `MemoryCapture` and `Capture` refuse an out-of-range
    count instead of wrapping it.** `read(2**64 + 5)` used to drain five
    records and `set_now(-1)` to stamp 2\*\*64 - 1; both now raise
    `OverflowError`, as do `Telemetry(-4)` (was `ValueError`) and a bad
    `block_samples`. `stats()` takes no arguments. jm now owns all three
    bindings; `FFT` keeps its hand-written binding with jm's output guards
    added. [#2971](https://github.com/doppler-dsp/doppler/pull/2971)
