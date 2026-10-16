- **A C benchmark can no longer lose a row without a word.** The vendored
    `jm_bench.h` dropped every row past 32 after printing it; jm 0.106.0's
    copy grows the table and fails the run instead (just-makeit#2188). The
    rows the cap kept out are back: the Spectrogram bench goes from 32 rows
    to 53 and PSD's from 32 to 34, both of which sat at the cap. `make   bench-interleaved` replays a failed run's stderr instead of discarding
    it, and `make bench` exits non-zero when its Python half fails to
    collect or a benchmark raises (#1977) (#2062).
