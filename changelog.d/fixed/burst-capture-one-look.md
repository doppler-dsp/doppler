- **`BurstCapture` refined to the wrong code period at its default sizing.**
    The burst sizer escalated `n_noncoh` past refine's reach, so a
    default-built capture reported `preamble_start` 9 and 3 periods late
    on a 34 dB burst, with a margin that read *better* than a correct one.
    A burst engine now never buys non-coherent looks (a burst has one
    frame of preamble), and the capture refuses a pinned grid beyond its
    reach. [#1181](https://github.com/doppler-dsp/doppler/issues/1181).
