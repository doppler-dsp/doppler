- **`Correlator` takes no rate or design point by omission.**
    `Acquisition`'s `sample_rate=4e6`, `chip_rate=1e6`, `symbol_rate=1000`
    and `cn0_dbhz=50` defaults and `BurstAcquisition`'s `fs=1.0` are gone: a
    search refuses a rate it reads that is not given, and `cn0_dbhz`
    defaults to NaN (no design point), which the continuous search refuses.
    `sample_rate=1.0` still gives normalized units
    ([design §17](docs/design/correlator-python.md)).
