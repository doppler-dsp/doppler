- **A ring buffer can be any size, and `capacity` is exactly what you asked
    for — on every machine.** A power of two is no longer required, and a
    sub-page request is no longer rounded **up** and reported: `F32Buffer(512)`
    was 512 on Linux x86, 2048 on macOS arm64 and 8192 on Windows. The
    rounding a mask and a page mirror need now happens in the mapping
    (`->mask + 1`), where it costs address space (under 2×) and nothing per
    call — measured. Code that asked for `1` to get "the page minimum", or
    indexed with `capacity - 1`, must use the size it wants and `->mask`.
    A file-backed ring's file is sized by the mapping, not the capacity
    ([measurements §7](docs/design/ring-buffer-measurements.md)).
