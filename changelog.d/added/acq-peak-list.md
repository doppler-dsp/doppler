- **`Acquisition.set_max_peaks(n)` / `BurstAcquisition.set_max_peaks(n)`
    — the peak list.** A dwell reports every peak above the same gate,
    strongest first, with an exclusion zone of one Doppler bin by one chip
    around each and the two-epoch rule for a peak at an already-listed code
    phase (a data-split twin is held one dwell and listed only if it recurs
    at the same tile); each listed peak is one `push()` record. One
    argmax now serves both detectors (`det_peak_list` in `det_private.h`).
    Measured at the operating point (`validate_acq_peak_list`,
    [design §12.6](docs/design/async-dsss-receiver.md)); the false-alarm
    rate under the list is the configured pfa. State blob v2.
