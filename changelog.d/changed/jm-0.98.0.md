- **just-makeit pin 0.96.0 → 0.98.0.** Every array parameter's stub now
    states exactly its declared `npt.NDArray` type (just-makeit#1724), and an
    array argument refuses a `str` only where its param declares `str_hint`
    (just-makeit#1824). 0.97.0 is included.
