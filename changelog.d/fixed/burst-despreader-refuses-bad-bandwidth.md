- **`BurstDespreader` refuses a `bn_carrier` or `bn_code` outside the loop
    filter's domain** (NaN, infinite, negative, or so large that its loop
    gains overflow; #2071): the constructor and both setters raise
    `ValueError`, and a refused setter keeps the loop's bandwidth.
