- **just-makeit pin 0.75.2 → 0.75.3: record types exist at runtime.** A
    `single = true` record (`ReceiverStatus`, `BerInterval`, `ToneMetrics`) is
    now created at module init and registered under its public name
    ([just-makeit#1264](https://github.com/just-buildit/just-makeit/issues/1264),
    filed from here), so `isinstance` and a docs directive bind to what the
    stub already declared.
