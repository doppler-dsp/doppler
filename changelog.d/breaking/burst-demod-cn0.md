- **`BurstDemod.est_snr_db` and `DsssBurstReceiver.est_snr_db` are gone,
    replaced by `est_cn0_dbhz` / `demod_cn0_dbhz` in dB-Hz.** The old field
    was never an SNR: it published the preamble estimator's spectral
    prominence, which carries the coherent processing gain and reads 59 dB on
    a 25 dB link. Renamed rather than redefined, so a threshold written
    against the old number fails loudly instead of silently changing meaning
    ([#1304](https://github.com/doppler-dsp/doppler/issues/1304)). The
    receiver's state blob is version 6.
