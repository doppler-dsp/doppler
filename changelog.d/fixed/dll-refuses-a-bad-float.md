- **The DSSS receivers, `Dll`, `Despreader` and the shared loop-filter loops
    refuse a bad argument with `ValueError` at create and on `configure`,
    instead of aborting or running NaN gains.** A refused retune or forged
    blob changes nothing; a seed past half the sample rate is refused (#2103).
    `RateSync` `configure`/`set_bn` stay void (#2112).
