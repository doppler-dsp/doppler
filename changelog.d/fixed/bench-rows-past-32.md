- **A C benchmark can no longer lose a row without a word.** The vendored
    `jm_bench.h` dropped every row past 32 after printing it; jm 0.106.0's
    copy grows the table and fails the run instead (just-makeit#2188). The
    rows the cap kept out are back, so the Spectrogram bench goes from 32
    rows to 53 and PSD's from 31 to 34. `make bench-interleaved` now
    replays a failed run's stderr instead of discarding it
    ([#2062](https://github.com/doppler-dsp/doppler/issues/2062)).
