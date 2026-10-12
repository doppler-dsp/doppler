- **PSD's windows have C names: `DP_PSD_WINDOW_HANN`, `_KAISER`,
    `_BLACKMAN_HARRIS` and `_RECT`.** The Python strings bind to them, and to
    `ACC_TRACE_*` and `DP_SPECTROGRAM_*`, by name; `make lint` refuses a number
    in those slots of a direct call to the five constructors (#1894).
