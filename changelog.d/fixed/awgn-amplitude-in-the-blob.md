- **`AWGN`'s state blob carries its seed and amplitude (v2, 60 B, from
    304 B), and refuses a bad one.** A restored generator keeps the
    source's amplitude and the seed `reset()` replays, so a restored `wfm`
    synth emits the source's noise level and replays the source's noise seed,
    not its own `snr` and seed (#2084). A NaN or negative amplitude is refused
    at create (`ValueError`), on restore and by the setter, which ignores it.
