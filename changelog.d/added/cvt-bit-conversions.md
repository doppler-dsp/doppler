- **`cvt` gains the six conversions a frame field is built from**, all
    jm-generated and callable from Python: `int_to_bin` / `bin_to_int` for a
    literal that fits in 64 bits, `hex_to_bin` / `bin_to_hex` for one that
    does not or that arrives as text, and `bin_to_nrz` / `nrz_to_bin` for the
    bit-to-symbol map. Bit order is numpy's `bitorder`, deliberately not the
    BLUE writer's `endian` (`EEEI`/`IEEE`) — bit order and byte order are two
    axes that share a word. The NRZ sign convention is not restated here: its
    home is BPSK in `mpsk_core.h`, and the C test asserts the two agree.
