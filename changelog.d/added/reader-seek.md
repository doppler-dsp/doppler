- **`wfm.Reader.seek(index)`, `seek_time(seconds)` and `position`** give a
    capture random access. Reaching sample N used to cost a full decode of the
    N samples in front of it; `seek()` is one file seek for raw, BLUE and
    SigMF, and a forward scan for CSV, which is delimited rather than strided
    ([#1333](https://github.com/doppler-dsp/doppler/issues/1333)). Out of range
    raises rather than landing quietly at EOF, and a refused seek does not move
    the read position. `seek_time()` converts through `fs` and **refuses** when
    `fs_source` is `"none"` — every raw and CSV capture — because the same
    arithmetic by hand lands on sample 0 for every time, silently.
