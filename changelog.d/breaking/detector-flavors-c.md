- **`CorrDetector` and `CorrDetector2D` are the correlator's detector
    flavors** (#2752 step 3.3). Saved `get_state()` blobs from
    `CorrDetector`, `CorrDetector2D` and `CarrierAcquisition` are refused
    (`DET1`/`DET2` retire for `CORL`). In C, every `dp_detector_*` and
    `dp_detector2d_*` symbol is `dp_correlator_*` (`create_detector_1d`,
    `create_detector_2d`), both write `dp_correlator_hit_t`, whose `col` is
    the 1-D lag, and the headers `doppler/detector/detector_core.h` and
    `doppler/detector2d/detector2d_core.h` are gone, from `doppler.h` too.
    Python's signatures are unchanged; the threshold now travels in a blob.
