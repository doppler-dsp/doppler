- **`snr_mode` and `modulation` are named in C, not spelled as integers.**
    A downstream setting `wfm_source_t` had to write `snr_mode = 3` from a
    comment — and the scale a dB figure is quoted on moves the noise by
    10log10(sps). `wfm_snr_mode_t` and `wfm_bitmod_t` join `wfm_type` and the
    rest under `make lint-wfm-enum-tables`, which holds every C enum's indices
    to the `[[enum]]` manifest and the `wfm_names.h` table.
