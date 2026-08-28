- **The sanitizer job is three parallel legs, not three sequential steps.**
    Each rebuilds the tree with its own flags, so the job's wall clock was
    their sum and it was the long pole on every PR — 27m02s on one measured
    run. TSan is about half of that on its own, so the split buys ~1.8x, not
    3x. `fail-fast: false`, so a failing ASan no longer hides what UBSan and
    TSan would have said.
