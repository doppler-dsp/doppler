- **A `str` passed to `Frame`/`FrameDesc` as bits now names
    `field_bits()`.** `preamble=`, `sync=`, `payload=`, `add_field`'s
    `bits` and `rx_bits` on `crc_ok`/`deframe`/`check` still raise
    `TypeError`. The message now ends with the reason a composer source
    field gives: *a bit field takes bits (a uint8 array); build them from
    text with field_bits()* (doppler#1708).
