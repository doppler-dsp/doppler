- **C: `CorrDetector` and `CorrDetector2D` are the correlator's detector
    flavors** (#2752 step 3.3). Every `dp_detector_*` and `dp_detector2d_*`
    symbol is now `dp_correlator_*`, built by `create_detector_1d` and
    `create_detector_2d`, and both write one hit record,
    `dp_correlator_hit_t`, whose `col` is the 1-D lag. Their blobs are
    `CORL`, so a `DET1` or `DET2` blob is refused; `CAQR` nests the new one.
    The threshold now travels in the blob. Python's classes keep their
    signatures; their method docstrings are the correlator's shared ones.
