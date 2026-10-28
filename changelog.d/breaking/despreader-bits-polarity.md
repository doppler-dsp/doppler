- **`Despreader.bits()` and `BurstDespreader.bits()` follow the library's
    one bit polarity.** Old: a positive prompt decoded as 1, the complement
    of every other decision in the library. New: bit `b` is sent as
    `1 - 2b` (`mpsk_core.h`), so a negative prompt is 1 and zero or positive
    is 0, the same decision as `nrz_to_bin`. A stream decoded before this
    release now comes back complemented. Code that inverted it to compensate
    should stop, and a `min(e, 1 - e)` score at a known carrier phase can
    become a plain `e` (#2654).
