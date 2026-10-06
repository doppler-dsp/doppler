- **Six more binding fragments are jm's** (#1446). `AWGN`, `CIC`,
    `DopplerChannel`, `Interrupt`, `Gold` and `PolynomialPhaseEstimator` are
    re-rendered by jm and leave the `-Wall -Wextra` exempt list. `AWGN`,
    `CIC`, `DopplerChannel` and `Gold` also gain the output-buffer overrun,
    overflow and `PyArray_SetBaseObject` guards that the hand-owned wrappers
    lacked.
