- **Telemetry's bindings raise `OverflowError` for an out-of-range count.**
    `Telemetry(-4)` and a bad `block_samples` raised `ValueError`; they, and
    `read`, `read_dict`, `set_now` and `records` given a count outside its
    C type, now raise `OverflowError`. `Telemetry.stats()` takes no
    arguments and refuses one with `TypeError`, where it ignored it
    ([#2972](https://github.com/doppler-dsp/doppler/pull/2972)).
