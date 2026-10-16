- **The benchmarks page reads in MSa/s, with your headroom.** Every
    stream kernel reads in input samples per second against a sample rate
    you enter; bit, chip and symbol rows read in their own rate, setup in
    time per call. Rows sit in family tabs with search, five sorts and a
    trend chart per row, named from `benchmarks/labels.toml`, which
    `make lint` requires for every new row
    ([#2372](https://github.com/doppler-dsp/doppler/issues/2372)).
