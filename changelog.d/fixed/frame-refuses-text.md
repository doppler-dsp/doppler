- **`Frame`/`FrameDesc` refuse a `str` where they take bits.** Their
    bindings were hand-owned and read `Frame(sync="0101")` as the number
    101, then failed later with an unrelated reason. Regenerated, every
    array argument (`preamble=`, `sync=`, `payload=`, `add_field`, and
    `crc_ok`/`deframe`/`check`'s `rx_bits`) now raises `TypeError` naming
    the parameter, and takes `bytes` as its stub already said. Make bits
    from text with `field_bits()` (doppler#1654).
