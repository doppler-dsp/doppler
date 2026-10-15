- **`BurstDespreader` refuses a NaN, infinite or negative `bn_carrier` or
    `bn_code`** (#2071): the constructor and both setters raise `ValueError`,
    and a refused setter keeps the loop's bandwidth.
