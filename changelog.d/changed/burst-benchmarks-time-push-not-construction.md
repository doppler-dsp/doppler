- **The burst chain's benchmarks time `push`, and construction has its own
    row.** The `BurstAcquisition`, `BurstCapture`, `BurstDespreader` and
    `DsssBurstReceiver` rows built the object inside the timed call. Since
    #1503 sizes the search on the burst, construction takes about 6.4 ms,
    and it read as a 5–6× slower idle push. The idle push is unchanged at
    about 1.2 ms. **Per burst, the capture's refine costs ~30× what it did
    in v0.54.0**: ~765 µs against ~25 µs at the benchmark's geometry. That
    is the price of scoring every repetition coherently (#1508) and every
    detected phase (#1520); making it cheaper is
    [#1538](https://github.com/doppler-dsp/doppler/issues/1538).
- **`make bench-interleaved` refuses a machine that is not ready.** Every
    core's governor and the ACPI platform profile must be `performance`, and
    the 1-minute load under 1.0. The snapshot now records the profile.
