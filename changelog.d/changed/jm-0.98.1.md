- **just-makeit pin 0.98.0 → 0.98.1.** Generated CPython glue builds clean under
    `-Wall -Wextra` (gh-1856, filed from doppler's warning sweep). In this tree
    `jm apply` re-rendered three files (the `PyCFunction` casts), taking the
    glue from 132 to 125 warnings under gcc 16 and from 322 to 315 under
    clang 23. The rest is in the 99 hand-owned `_ext_<obj>.c` fragments and
    the hand-written `stream_ext.c`, which jm never re-renders.
