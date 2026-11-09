- **`Telemetry`, `MemoryCapture` and `Capture` refuse an out-of-range
    count instead of wrapping it.** `read(2**64 + 5)` used to drain five
    records and `set_now(-1)` to stamp 2\*\*64 - 1. jm now owns all three
    bindings; `FFT` keeps its hand-written binding with jm's output guards
    added, and its `out=` path checks `PyArray_SetBaseObject` as jm's does. [#2972](https://github.com/doppler-dsp/doppler/pull/2972)
