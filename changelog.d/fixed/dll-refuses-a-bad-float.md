- **The DSSS receivers, `Dll`, `Despreader` and the shared loop-filter loops
    refuse a bad float or out-of-domain argument instead of aborting or
    running NaN gains.** A refused retune, a forged blob or a seed past fs/2
    changes nothing. Chain sizes past allocation still abort (#2112).
    `RateSync` `configure`/`set_bn` stay void (#2112).
