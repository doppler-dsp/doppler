- **`AsyncDsssReceiver` and `AsyncDsssPool` refuse a design C/N0 whose
    carrier lock cannot be sized.** No count up to 10^7 updates detecting at
    Pd 0.99 against Pfa 1e-3 (below about 5 to 10 dB-Hz at 415.1-B-1) raises
    `ValueError`; the count is `carrier_lock_n_noncoh` (33 / 43 at the floor)
    ([async-dsss-receiver §4.10](docs/design/async-dsss-receiver.md)).
