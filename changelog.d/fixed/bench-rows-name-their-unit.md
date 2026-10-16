- **A C benchmark row names its unit.** Each C row now records what it
    counts (call, sample, frame, bit, ...), and its time is per that unit,
    not per round: `acc_f32::step`, 65,536 calls a round, takes 0.6 ns per
    sample, where the page used to print 39 µs "per call". The `bench-timer`
    gate refuses a row recorded without its unit
    ([#2314](https://github.com/doppler-dsp/doppler/issues/2314)).
