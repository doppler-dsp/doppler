- **`AsyncDsssReceiver`'s loops show their filter terms at their own
    cadence**: the code loop's per-steer set (`rx.code.steer.*`), the timing
    loop's `prop` and `integ`, and the symbol-lock EMAs. They observe and
    decide nothing (the probe list:
    [`dp_async_dsss_receiver_set_telemetry()`](docs/c-api/async__dsss__receiver__core_8h.md)).
