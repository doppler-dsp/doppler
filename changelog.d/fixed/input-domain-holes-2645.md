- **Constructors and parsers refuse what they cannot represent.** Every
    converter's `create()` refuses a NaN or infinite scale (Python
    `ValueError`), and a NaN sample latches `clipped`; `InterpolatedTable`
    and `Farrow` refuse an unknown method or order in C; `dp_isotime_parse`
    refuses `2026-02-31` and `2025-02-29`; `proc_gain(nfft < 2)` is NaN;
    `CarrierMpsk(tsamps=0)` raises; a `Plan` refuses a seed above `2**32 - 1`
    and an override it cannot honour (unknown key, wrong-length array,
    non-number entry) instead of rendering the baseline (#2645).
