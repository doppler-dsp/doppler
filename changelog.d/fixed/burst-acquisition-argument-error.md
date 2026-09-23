- **`BurstAcquisition` raises `ValueError` for a bad argument, not
    `MemoryError`.** Its constructor returns NULL only for an argument it
    refuses (an infinite `cn0_dbhz`, `pfa` outside (0, 1), `reps < 1`, a
    silent preamble), and the message now names the constraints, as
    `BurstCapture`'s does. Closes
    [#1486](https://github.com/doppler-dsp/doppler/issues/1486).
