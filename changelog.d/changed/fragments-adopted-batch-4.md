- **Twenty-nine more binding objects are jm's** (#1446). The accumulators,
    `ddc`/`ddcr`, `nco`, `lo`, `delay`, the ring buffers, `viterbi`,
    `rs_codec`, the measurement objects and others are now re-rendered by jm
    and leave the `-Wall -Wextra` exempt list, which is down to 35 files.
    They gain the output-buffer overrun, overflow and `PyArray_SetBaseObject`
    guards the hand-owned wrappers lacked; no docstring changed.
