- **The benchmarks page reads in MSa/s.** Every stream kernel reads in
    input samples per second, whatever its own unit; bit, chip and symbol
    rows read in their own rate, and setup and read-outs in time per call.
    Enter your sample rate and each stream row shows its headroom, the
    ×-margin over real time on the measured machine. Twelve headline
    kernels open the page; below them every benchmark sits in a family
    tab, with search, a C/Python filter and five sorts, and a row opens a
    chart of both builds across releases that breaks where its round
    changed size. Names, units and definitions come from
    `benchmarks/labels.toml`, and `make lint` refuses a new published row
    without one ([#2372](https://github.com/doppler-dsp/doppler/issues/2372)).
