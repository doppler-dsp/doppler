- **`RateConverter.set_state()` reports a stage that refuses its part of
    the blob.** It reported such a blob as restored; it now raises
    `ValueError`, though earlier stages may already be restored (#2104).
