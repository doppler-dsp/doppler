- **`doppler.pc` resolves its prefix under a multiarch libdir.** The
    relocatable prefix was a literal `${pcfiledir}/../..`, right for `lib/`
    and wrong for Debian's `lib/x86_64-linux-gnu/`, where
    `pkg-config --cflags` pointed at `/usr/lib/include`. The hop count is
    now derived from the libdir. Found by the new package install test.
