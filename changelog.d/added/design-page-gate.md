- **A design page states what IS, and `make lint` now says so.** Three
    heading-and-preamble patterns — a dated section, a `**Status:**` or
    `*Phase N*` preamble, a record-family heading — fail
    `design-pages-check`; the companion `-measurements.md` is exempt by name,
    so a new one is covered the moment it exists. Ratcheted at 24 entries
    across 15 pages, failing on a **fixed** entry too so the list can only
    shrink ([#1302](https://github.com/doppler-dsp/doppler/issues/1302)).
