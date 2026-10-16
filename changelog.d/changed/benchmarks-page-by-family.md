- **The benchmarks page is readable now.** It opens with twelve headline
    kernels in plain words, each with its portable and native numbers and a
    trend across every release, then shows every benchmark one family at a
    time, in a table that scrolls in its own box with search, a C/Python
    filter and three sorts. A row opens a chart of both builds across
    releases. Every release is converted to one quantity, time per unit, so
    a trend is continuous; it breaks where a row's round changed size. Each
    row's plain name, unit and definition come from
    `benchmarks/labels.toml`, and `make lint` refuses a new published row
    without one ([#2372](https://github.com/doppler-dsp/doppler/issues/2372)).
