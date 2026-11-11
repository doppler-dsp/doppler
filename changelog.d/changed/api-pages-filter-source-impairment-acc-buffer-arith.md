- **`doppler.filter`, `doppler.source`, `doppler.impairment`,
    `doppler.accumulator`, `doppler.buffer` and `doppler.arith` are
    documented by the generated Python reference.** Each package's overview
    and worked examples open its reference page and run as tests, the ring
    buffers' threaded and single-threaded surfaces are a guide,
    [Ring Buffers](docs/guide/ring-buffers.md), and the six hand-written API
    pages are gone. Two package summaries were wrong and are corrected:
    `DopplerChannel` adds no noise, and `AccF32` sums real values (#3000).
