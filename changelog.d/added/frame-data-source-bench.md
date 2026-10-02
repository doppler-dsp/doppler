- **The frame benchmark times a frame pulled from a data source.** Five
    new `bench_frame_core` rows cover the per-frame path a `data:LEN` frame
    takes: a pull from `pn:0` or a pipe, plain assembly, and CADU assembly
    with RS + randomise + conv. A CADU costs 20–25 µs, about 11 % of a
    frame at one sample per bit, so assembly does not need to run ahead of
    the pacer. The numbers are in
    [the measurement record, §6](https://doppler-dsp.github.io/doppler/design/payload-data-source-measurements/#6-unknowns-the-two-throughput-ones-measured-2026-10-02).
