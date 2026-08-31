- **A new validator is now gated the moment its folder exists, as the process
    page always claimed.** The tree-wide limits gate carried a hand-written
    registry while `make validate-check` globbed, so a validator could render
    its report, pass both the staleness and report-format gates, and have
    every one of its limits asserted by nobody — three green gates and an
    unasserted envelope. It discovers by glob now, and fails loudly on an
    empty match. Closes
    [#1144](https://github.com/doppler-dsp/doppler/issues/1144).
