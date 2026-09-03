- **just-makeit pin 0.75.2 → 0.75.4: record types exist at runtime.** A
    `single = true` record (`ReceiverStatus`, `BerInterval`, `ToneMetrics`) is
    created at module init and registered under its public name
    ([#1264](https://github.com/just-buildit/just-makeit/issues/1264), filed
    from here), so `isinstance` and a docs directive bind to what the stub
    declared. 0.75.3 was held: a view and its parent sharing a record name
    registered two types and freed one
    ([#1268](https://github.com/just-buildit/just-makeit/issues/1268),
    a segfault `make test-stubs` caught); 0.75.4 aliases them to one type and
    carries a record's docs to the runtime face ([#1267](https://github.com/just-buildit/just-makeit/issues/1267)).
