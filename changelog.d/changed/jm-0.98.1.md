- **just-makeit pin 0.98.0 → 0.98.1.** Generated CPython glue builds clean under
    `-Wall -Wextra` (gh-1856, filed from doppler's warning sweep). In this tree
    `jm apply` re-rendered three files (the `PyCFunction` casts); doppler's own
    C stays at zero warnings, and the glue is 125 under gcc 16 (was 132) and 312
    under clang 23 (was 322). The rest is in the 99 hand-owned `_ext_<obj>.c`
    fragments and the hand-written `stream_ext.c`, which jm never re-renders.
