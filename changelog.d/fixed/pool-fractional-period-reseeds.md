- **A tracking `AsyncDsssReceiver`'s `chip_phase` is at the stream's next
    sample, not its chain's.** It trailed by the partial period the chain
    carries: about 50 chips at 11 MSa/s, so the pool re-seeded every held
    signal as a duplicate ([#3063](https://github.com/doppler-dsp/doppler/issues/3063)).
