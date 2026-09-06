- **`add_stage`'s docstring described a string enum that raises.** It said
    `kind` *"names the transform — `crc16`, `rs`, …"*, left over from a
    conversion that was measured and reverted; `add_stage("crc16", …)` is a
    `TypeError`. It now names the constants above.
    [#1223](https://github.com/doppler-dsp/doppler/issues/1223).
