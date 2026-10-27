- **`design_lowpass` taps have unit DC gain, and eight stale texts match
    the code again.** The taps are now divided by their sum, so they sum to
    1 to float precision; their values change slightly (by up to a few
    percent on a short design). The text fixes: `wfm_synth`'s bit pattern is
    sent once, not cycled, and `snr_mode = auto` leaves dsss at full scale;
    `ppe` uses dechirp and FFT, not a HAF; `cic_core.c`'s header names the
    2.0 headroom and offset binary; `Reader.sample_type` lists the scalar
    types; a nearest tie in `interp_table` takes the floor; `rrc_taps` are
    real float32; the `snr_core.h` example compiles (#2653).
