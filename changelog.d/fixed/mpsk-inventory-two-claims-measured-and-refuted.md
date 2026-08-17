- **Two claims the inventory listed as unmeasured are now measured — and did not
    hold**, which the report says instead of leaving them open-ended.

    **C21**, the `A^2` timing under-drive with `agc = 0`: §2.9 shows a level
    error reaching `timing_rate`, but the proxy is **not monotone in level**. At
    25 dB Es/N0 and amplitude 0.25 the un-levelled receiver reads *better* (4 ppm
    against 5), so an assertion on it would have been true at one operating point
    and false at another.

    **C16**, that `num_phases = 64` is the *measured saturation point*: swept 4
    to 1024 arms at an off-grid rate, EVM is flat to **0.08 dB**. So this
    geometry saturates below 4 arms and does not locate 64 as the knee at all —
    worse than unmeasured, because the obvious test would have asserted a
    difference that is not there.

    Both need a harsher stimulus than a per-push validator builds, so they belong
    in `make characterize` or `native/validation/`
    ([`docs/dev/adding-algorithms.md`](docs/dev/adding-algorithms.md) phase 7).
    Recorded because "measured and refuted" and "not yet measured" are different
    states, and a reader deciding what to do next needs to know which one applies.
