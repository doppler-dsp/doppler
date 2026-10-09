- **CarrierAcquisition's block is at least 3 samples** (#1959). A
    `resolution_hz` above `fs / 2.5` used to get a 2-sample block. Under
    the default Hann window that read NaN, and AsyncDsssReceiver aborted
    on it once PSD refused the window. `AsyncDsssReceiver` also refuses a
    non-finite `symbol_rate`.
