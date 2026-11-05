- **The docs build no longer runs away.** A cold strict build grew past 4 GB
    with zensical 0.0.45 and mkdocstrings 1.0.4 and was OOM-killed at the
    shared guard's ceiling; the matched upgrade builds the same 918 pages in
    about 520 MB. `docs-check` now fails a build whose peak exceeds 1536 MB on any machine (a local 2G ceiling too), and the docs need Python 3.11 or newer. See
    [#2840](https://github.com/doppler-dsp/doppler/issues/2840).
