- **Published benchmarks are measured on one core class.** On a
    heterogeneous CPU an unpinned benchmark is bimodal (3.5 vs 5.6 µs for
    one `awgn` binary, Zen 5 vs Zen 5c), so a handful of the 465 lost the
    coin toss every pass and published a 1.6× "regression" with no source
    change. `bench-interleaved` now pins the measurement to the fastest
    class and records it in the snapshot.
