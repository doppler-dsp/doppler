- **`RateConverter.set_state()` reports a stage that refuses its part of
    the blob.** It reported such a blob as restored; it now raises
    `ValueError`. Earlier stages may already hold the blob's values, so
    discard the converter after that error and re-create it (#2104).
