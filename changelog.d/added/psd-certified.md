- **PSD is certified**: a generated validation report
    (`src/doppler/spectral/tests/validation/psd/results.md`) with 23 limits,
    each measured through the binding against an external truth, and 15
    findings. Seven were fixed by #1959; four stay open as gaps: a scalar
    readout's 0.0 is also a real 0 dB (#1957), adjacent bands double-count
    their shared bin (#1958), refusals reach Python as a bare `MemoryError`
    (#1986), and the stubs omit `| None` (#2001). Part c of #1911.
