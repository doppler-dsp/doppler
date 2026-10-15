- **`_SynthEngine`'s constructor takes the manifest's positional order, and
    `seed` defaults to 1** (`type, fs, freq, snr, snr_mode, …`, was
    `type, snr_mode, fs, …`; the documented default was always 1, the code's
    was 0; #2335). Keywords bind as before, so pass its arguments by keyword,
    and pass `seed=0` where code relied on the old default.
