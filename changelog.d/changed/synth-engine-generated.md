- **`_SynthEngine`'s binding is jm's now, and its constructor takes the
    manifest's positional order** (`type, fs, freq, snr, snr_mode, …`, was
    `type, snr_mode, fs, …`; keywords bind as before). `seed` defaults to the
    documented 1, not 0 (#2335). `prod` refuses a view or a wide block like
    `Synth.prod`, `pn_poly` and `set_chirp_span` refuse an out-of-range
    integer instead of wrapping it (#2279), and `close()` waits for a running
    `prod`. The six `set_*` setters keep their behavior and gain stubs and
    Parameters docs; three of their messages no longer name a wrong waveform
    type, which the C treats as a no-op.
