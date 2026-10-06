- **`Interleaver` / `Deinterleaver` are jm-generated; a partial block is a
    declared refusal** (doppler#1446). The `ValueError` for a length that is not
    whole blocks was hand-written six times (per method, per call path); it is
    now `error_on_empty` in the manifest, and the stub documents `Raises:   ValueError`. The message names the `block_bits` property instead of quoting
    its value, so the certified limit reads "names block_bits" now. Exempt
    list 15 to 13.
