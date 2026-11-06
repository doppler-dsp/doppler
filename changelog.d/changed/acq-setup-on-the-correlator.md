- **`Acquisition`'s template shape and off-peak floor come from the
    correlator** (#2752 step 3.4a): their autocorrelation runs on the
    correlator's single-row path, in cf32, instead of a private double
    transform pair. The reported peak sidelobe (`psl_db`) moves by at most
    6.2e-6 dB; every acq validator's verdict is unchanged. A new benchmark
    row times `AsyncDsssReceiver` create on a five-tile search.
