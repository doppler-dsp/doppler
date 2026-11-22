- **`AsyncDsssReceiver`'s loops show their filter terms at their own
    cadence**: the code loop's per-steer set (`rx.code.steer.*`), the timing
    loop's `prop` and `integ`, and the symbol-lock EMAs. They observe and
    decide nothing
    ([async-dsss-receiver §4.11](docs/design/async-dsss-receiver.md)).
