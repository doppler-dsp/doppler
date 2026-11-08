- **`BurstCapture` calls the burst search directly**, not through the
    `burst_acq` forwarder that step 4 retires
    ([design §8](docs/design/correlator-python.md)). Its state blob is
    byte-identical and its tests unchanged; `DsssBurstReceiver` no longer
    links `burst_acq` (#2752).
