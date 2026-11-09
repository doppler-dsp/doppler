- **`dp_correlator_bank_power_max` and `dp_correlator_bank_detect` take an
    `int *status` ahead of `why`**: `DP_ERR_MEMORY` when memory ran out,
    `DP_ERR_UNSUPPORTED` off a map, `DP_ERR_INVALID` for a refused
    argument; pass NULL to ignore it. In Python a bank that cannot allocate
    raises `MemoryError` (was `ValueError`), and `bank_detect`'s work
    buffers are a refusal where they aborted
    ([design §18](docs/design/correlator-python.md)) (#2752).
