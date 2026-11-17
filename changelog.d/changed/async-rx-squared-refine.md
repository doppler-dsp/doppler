- **The `AsyncDsssReceiver`'s refine estimates the carrier on the squared
    stream too.** Squaring the despread samples removes the data, so a
    dwell over data and one over the code-only window both show a tone at
    twice the residual; that estimate is handed over when it is found, the
    data-template one otherwise. At the 415.1 and 3.069 Mcps points, 4 to
    10 dB, static and at 400 Hz/s, the hand-over's median error falls from
    19-98 Hz to 3-22 Hz, and its worst case falls in every row.
    Its threshold is derived for the tone template, not fitted
    ([DSSS receivers](docs/guide/dsss-receivers.md), #3107).
