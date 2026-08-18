- **`track` gains a characterization tree, and the M-PSK pull-in envelope
    becomes a curve anybody can re-run**
    ([#849](https://github.com/doppler-dsp/doppler/issues/849)).
    `src/doppler/track/tests/characterization/pull_in/` sweeps the success
    fraction against multiples of each loop's own acquisition bound, across
    every constellation order and two oversampling ratios, composed entirely
    from the shipped harness.

    It existed as dated prose in three docstrings before this, and re-derived
    by nothing — which is how two findings came to be filed against the
    receiver for behaviour that was really a test seeded past the bound
    (#843, both retracted). The docstrings now cite the subject instead of
    quoting numbers.

    Two things the sweep establishes beyond the shoulders: the collapse
    multiple does **not** move with `sps` (identical rows at 8 and 16), which
    is the check that the bound really is stated in cycles per symbol; and it
    barely moves with `m` (4/4/3), which is the `1/m` being carried correctly
    — a missing `m` would spread the collapse fourfold across the orders.
