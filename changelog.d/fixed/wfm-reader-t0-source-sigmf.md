- **`Reader.t0_source` no longer raises on a SigMF capture that declares a
    start time.** The getter range-checked against a hand-copied table of two
    names, so `"sigmf"` (the third) failed with
    `ValueError: ... (valid: 0..1)`. The table is rendered from the `t0_source`
    `[[enum]]` now, which is how it stays the length of the enum (doppler#1446).
