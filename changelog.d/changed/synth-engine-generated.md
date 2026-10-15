- **`_SynthEngine`'s binding is jm's now.** `prod` refuses a strided or
    non-complex64 block like `Synth.prod`, `pn_poly` and `set_chirp_span`
    refuse an out-of-range integer instead of wrapping it (#2279), and
    `close()` waits for a running `prod`. The six hand-written setters
    (`set_rrc`, `set_bits`, `set_symbols`, `set_dsss_chips`,
    `set_dsss_cont`, `set_dsss_window`) keep their behavior and gain stubs
    and Parameters docs; three of their messages no longer name a wrong
    waveform type, which the C treats as a no-op.
