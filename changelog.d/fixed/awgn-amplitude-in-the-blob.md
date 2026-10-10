- **`AWGN`'s state blob carries its seed and amplitude (v2, 60 B, from
    308 B).** `reseed` and the `amplitude` setter change both after
    `create`, so a restored generator used to keep its own amplitude and
    reset to its own seed. The never-read AVX2 stream words are gone. A
    restored `wfm` synth now emits the source's noise level, not its own
    `snr` (#2084).
