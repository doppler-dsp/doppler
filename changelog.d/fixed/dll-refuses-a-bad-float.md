- **`Dll`, `Despreader`, `LoopFilter` and the DSSS receivers raise a
    `ValueError` naming a bad argument, instead of crashing or aborting the
    interpreter** (a NaN spacing or rate, `sps=1`, a `pfa` of 2, a tiny
    carrier). Changed: `MemoryError` becomes `ValueError`;
    `set_symbol_period(-inf)` and a period past 2^20 partials are refused; a
    coupled `carrier_freq_hz` must be 0 or above half the sample rate; the
    `Dll.bn` and `Despreader.bn_code` setters refuse without raising until
    just-buildit/just-makeit#2182 (#2103).
