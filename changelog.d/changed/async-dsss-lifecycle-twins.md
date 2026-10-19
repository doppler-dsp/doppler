- **The `AsyncDsssReceiver` lifecycle example's Python and C faces run one
    draw.** The C twin takes the Python twin's seeds and its sigma function,
    `dp_wfm_awgn_amplitude()`. Both print the same five lines, one a stage,
    and `test_c_example_pairs.py` runs both and requires them to match
    ([the gallery page](../../docs/gallery/async-dsss-receiver-lifecycle.md)).
