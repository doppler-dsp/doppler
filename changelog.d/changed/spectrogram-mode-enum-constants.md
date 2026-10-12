- **`DP_SPECTROGRAM_POWER` and `DP_SPECTROGRAM_DB` are enum constants, no
    longer macros.** Their values are unchanged (0 and 1), but `#ifdef`,
    `#if defined (...)` and `#if DP_SPECTROGRAM_DB` on them now read false or 0
    (#1894).
