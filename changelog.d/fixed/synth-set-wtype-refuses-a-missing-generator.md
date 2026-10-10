- **`_SynthEngine.set_wtype` refuses a type its engine cannot run.**
    Switching an engine created as a tone (or any non-PN type) to PN, BPSK
    or QPSK stored the type, and the next step read a PN generator
    `create()` never built: a segfault. `set_wtype` now raises
    `ValueError` for that, and for a value that is not a waveform type,
    and leaves the type unchanged (#2095).
