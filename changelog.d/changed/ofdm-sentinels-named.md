- **`ofdm`: the two `2**32 - 1` sentinels are named, and the refusals the
    suite never took are tested.** `add_fixed`'s every-phase value and
    `add_run`'s keep-the-prefix default are documented as `OFDM_ALL_PHASES` and
    `OFDM_CP_DEFAULT` (both 2\*\*32 - 1, asserted; just-makeit#2679 owns a
    named default); a partial `finish` clears the rest of the tail. See the [OFDM guide](docs/guide/ofdm.md).
