- **`BurstDespreader` refuses a NaN, infinite or negative `bn_carrier` or
    `bn_code`** instead of running with NaN loop gains from the first symbol.
    The constructor raises `ValueError`; the setters refuse and keep the
    loop's bandwidth, without raising until just-buildit/just-makeit#2182
    (#2071).
