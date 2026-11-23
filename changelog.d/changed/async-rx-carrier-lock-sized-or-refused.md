- **`AsyncDsssReceiver` and `AsyncDsssPool` size the carrier lock at the
    bandwidth the carrier loop delivers, and refuse a C/N0 it cannot be
    sized at.** `carrier_lock_n_noncoh` is 34 / 43 at the 415.1-B-1 floor
    (2.2 / 2.8 GHz), detecting at Pd 0.99 against Pfa 1e-3; past 10^7
    updates, `ValueError`
    ([async-dsss-receiver §4.10](docs/design/async-dsss-receiver.md),
    #3285).
