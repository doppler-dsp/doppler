- **`RateConverter.set_state()` reports a stage that refuses its part of
    the blob.** It discarded each stage's own refusal, so a blob a stage
    refused was reported as restored. It now raises `ValueError` (#2104).
