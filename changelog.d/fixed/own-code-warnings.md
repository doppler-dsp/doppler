- **doppler's own C builds with no `-Wall -Wextra` warning from clang, and
    the gcc ones are down to the uninitialised reads fixed in #1831** (#1658).
    None was silenced with a flag, pragma or blanket cast; each was fixed at
    its cause. Two were findings: `carrier_mpsk_jitter` computed whether
    jitter scales with `bn` and never checked it (the validation could not
    fail on it; it is now wired in), and making `det_private.h`'s helpers
    `static inline` showed gcc a `NULL`-scratch median path in `acq`'s tile
    decide, now excluded in the code instead of by a caller's say-so.
