- **A C benchmark row now reads per unit, not per round.** The benchmark
    page printed each C row's round time as "time/call", so
    `acc_f32::step`, 65,536 calls per round, read 39 µs for a 0.6 ns call.
    Every C row now names what it counts (call, sample, frame, bit, ...),
    the page prints the time per that unit, and sample rows go back to the
    throughput table. A C row from v0.43.0 to v0.72.0, which recorded no
    unit, now reads "/ round of N" instead of claiming to be per call. The
    `bench-timer` gate refuses a row recorded without its unit
    ([#2314](https://github.com/doppler-dsp/doppler/issues/2314)).
