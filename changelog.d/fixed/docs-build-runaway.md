- **The docs build no longer runs away.** A cold strict build grew past 4 GB
    with zensical 0.0.45 and mkdocstrings 1.0.4 and was OOM-killed at the
    shared guard's ceiling; the matched upgrade builds the same 918 pages in
    about 520 MB, and the build now runs under its own 2G ceiling. See
    [#2840](https://github.com/doppler-dsp/doppler/issues/2840).
