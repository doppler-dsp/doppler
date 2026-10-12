- **PSD's windows have C names, and every integer-coded argument is written
    by one.** `DP_PSD_WINDOW_HANN`, `_KAISER`, `_BLACKMAN_HARRIS` and `_RECT`
    name the `window` of `dp_psd_create`, `dp_spectrogram_create` and
    `dp_carrier_acq_create`, as `ACC_TRACE_*` and `DP_SPECTROGRAM_*` already
    named the modes. The Python bindings now map each string to its constant
    by name (the `psd_window`, `acc_trace_mode`, `spectrogram_mode` and
    `carrier_acq_window` enums of `just-makeit.toml`), so the compiler checks
    the table, and `make lint` refuses a number in any of those slots
    (`scripts/check_named_args.py`; `make format` writes the names). No value
    changed: the built C objects are identical (#1894).
