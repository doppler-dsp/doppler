- **`BurstCapture`'s validation report certifies what a caller gets**
    ([#1517](https://github.com/doppler-dsp/doppler/issues/1517)). The report
    now has a §2.8 for the delivered Pd. Measured on a Zadoff-Chu 127 × 8
    preamble at every depth D = 1..8, the capture delivers at least
    `pd_burst`, and refine names the wrong repetition in 1.9% of trials. A
    limit holds that under 2.5%: it catches a refine regression that
    `pd_burst`'s margin would hide from the Pd check. `pd_burst`'s
    docstring now says so. It used to say the capture's losses were not
    priced in.
