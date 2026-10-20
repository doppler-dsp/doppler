- **The `Dll` state blob is version 15, and a blob saved by v0.73.0 or
    earlier is refused** (`set_state()` raises `ValueError`; C returns
    `DP_ERR_INVALID`), as is any blob that nests one: `Despreader` v5,
    `DsssReceiver` v3, `AsyncDsssReceiver` v10 and `AsyncDsssPool` v4. The
    blob now carries the output's level (#2495).
