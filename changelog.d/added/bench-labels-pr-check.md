- **A new benchmark row needs its label in the PR that adds it, and CI
    checks it.** `make bench-labels-pr-check` reads the rows the PR's own CI
    run measured, untimed: 601 C rows with their units and 288 Python rows
    today. It refuses one with no label, and a C row whose unit contradicts
    its label. Before this, v0.83.0's five unlabelled HalfbandDecimatorR2C
    rows were first seen on release day (#2390; the gaps left are #3039).
