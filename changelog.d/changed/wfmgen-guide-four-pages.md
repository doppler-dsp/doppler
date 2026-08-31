- **The wfmgen guide is four pages, not twelve** — one page per question a
    reader actually has: what the model is and how to run it (index), what you
    can generate (waveforms), how to put it in time and sweep it (scenes), and
    how to do it from Python. All 67 flags are still documented, now across 4
    pages instead of 12, and every removed page's anchors redirect. The
    **recipes now execute**: they had been exempt from the doc gate because two
    of the eight shared a fence with a `--continuous` command, and one of them
    (`--symbols-file qam16.cf32`) referenced a file nothing created, so it could
    not be copy-pasted at all.
