- **A DSSS burst's BER vs Eb/N0, through one `wfm.Plan`.**
    `dsss_burst_ber_demo.py` draws each trial with `plan.at(snr, seed)`,
    decodes it blind with `DsssBurstReceiver`, and scores the PN payload
    against its own Field. Every point's `BerMeter` interval must fall
    between ideal BPSK and the receiver's sync-phase-limited curve
    ([gallery](docs/gallery/dsss-burst-receiver.md#ber-vs-ebn0-through-one-plan), #1619).
