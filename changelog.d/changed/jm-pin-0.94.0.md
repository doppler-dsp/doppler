- **just-makeit pin 0.93.0 → 0.94.0.** Every generated array argument now
    refuses a `str` with a `TypeError` naming the parameter, and a `uint8`/
    `int8` array also takes `bytes`/`bytearray`/`memoryview` (just-makeit#1700).
    A self-sized output past `NPY_MAX_INTP` raises `OverflowError` rather than
    numpy's "negative dimensions" (just-makeit#1710).
