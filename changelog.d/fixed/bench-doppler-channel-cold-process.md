- **The `doppler_channel` C benchmark published a ratio its own prose
    contradicted, and now does not.** It reported `ramp/static = 0.70x` — "a
    drifting offset is 30% cheaper than a fixed one" — directly beside a
    paragraph asserting the two cost the same. The paragraph was right.

    The benchmark already warmed up per configuration, which warms the caches
    for the config it precedes. What it could not warm is the CPU's frequency
    ramp out of a cold process, and that is charged entirely to whichever
    configuration runs *first*. `MIN` over rounds cannot remove it either,
    because every round in a cold process is equally cold — the usual defence
    against a slow outlier is no defence against a slow *start*.

    A process-level warm-up before any configuration is timed takes the ratio
    to **1.00x**, reproducibly. An instance of the class filed as
    [#896](https://github.com/doppler-dsp/doppler/issues/896), fixed here for
    this benchmark; the general case is still open.

    Worth knowing when reading any multi-config benchmark in this tree: a
    ratio *below* 1.0 against the first row is the signature, and it is a
    measurement artifact rather than a finding.
