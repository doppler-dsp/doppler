- **The benchmarks page reads in MSa/s, with your headroom.** Every
    stream kernel reads in input samples per second against a sample rate
    you enter; bit, chip and symbol rows read in their own rate, setup in
    time per call. Rows sit in family tabs with search, five sorts and a
    trend chart per row, named from `benchmarks/labels.toml`; a row
    published for the first time needs a label, checked at release (per
    PR: #2390). It draws however the page is reached, by a link or a
    reload ([#2391](https://github.com/doppler-dsp/doppler/issues/2391))
    ([#2372](https://github.com/doppler-dsp/doppler/issues/2372)).
