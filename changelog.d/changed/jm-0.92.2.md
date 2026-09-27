- **just-makeit pin 0.92.1 → 0.92.2**, which closes the last of the `dp_`
    rename ([#1565](https://github.com/doppler-dsp/doppler/issues/1565)):
    `jm upgrade` now respells a container property's accessors
    (`dp_RateConverter_num_stages` / `_num_bank_shape`, jm#1695), and a
    `bridge_fn` may name a `dp_` function (`dp_wfm_source_to_synth`,
    jm#1694). Every symbol `libdoppler.a` and `libdoppler_stream.a` export now
    carries `dp_`, and `make symbol-prefix-check` enforces it with no
    allowlist.
