- **`mpsk_receiver_performance_demo.py` runs again, and measures with the
    library's own meters.** It carried three private copies of shared
    formulas. The carrier offset used `bn_carrier / sps` where the
    acquisition bound is `bn_carrier / m` cycles per symbol, seeding `0.5·m`
    times the bound — 4× at 8PSK, enough that trials ended mid-acquisition
    and read as lock failures. The EVM was longhand (now `ber_evm_db`,
    verified equivalent to 0.01 dB first). The SER was differential and taken
    as the minimum over 401 lags, which cannot see a cycle slip: it read
    0.055 where the coherent truth was 0.466. Off `.examples-skip`.
    [#1060](https://github.com/doppler-dsp/doppler/issues/1060)
