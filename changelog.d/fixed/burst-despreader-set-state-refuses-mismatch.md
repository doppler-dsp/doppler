- **`BurstDespreader.set_state()` refuses a blob from a differently
    configured despreader instead of crashing the next `steps()`.** It copied
    every field from the blob but kept its own code buffers, so a blob taken
    with `set_acq()` active, restored into a despreader without it, left a
    NULL acq code to read. The acq code and the loops' `bn` now travel; a
    blob restores only into a despreader with the same `sf`, `sps`, seeds
    and acq-code length, and a refused one, or one holding a NaN, changes
    nothing. An invalid constructor argument, now including a non-finite
    seed, raises `ValueError` instead of `MemoryError` (#2041).
