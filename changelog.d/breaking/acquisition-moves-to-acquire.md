- **The burst acquisition objects move from `doppler.dsss` to
    `doppler.acquire`** (#1511). Since #1470 they take any repeated complex
    preamble (Zadoff-Chu, a chirp, a QPSK sequence), and a PN code is just
    one case. `Acquisition`, `BurstAcquisition`, `BurstCapture`,
    `PersistentBurstCapture` and `bin_to_signed` now sit beside
    `CarrierAcquisition`. **Breaking for Python:** import them from
    `doppler.acquire` instead of `doppler.dsss`, for example
    `from doppler.acquire import BurstCapture`. `doppler.dsss` does not
    re-export them. The C API is unchanged, and so are the DSSS receivers
    that compose these objects.
