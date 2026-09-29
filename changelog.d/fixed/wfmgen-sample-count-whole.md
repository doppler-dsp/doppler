- **A negative sample count no longer writes without bound.** `--count`,
    `--off` and `--delay` took a negative value and wrapped it to 2^64, so
    `--count 64 --delay -1` ran until the disk filled. A fraction truncated
    silently. Each side of the value, or of a `LO:HI` range, must now be a
    whole, non-negative count; anything else exits 2 naming the flag. `1e3`
    still reads as 1000 (#1629).
