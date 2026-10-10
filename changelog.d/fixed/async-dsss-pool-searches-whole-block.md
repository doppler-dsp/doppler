- **`AsyncDsssPool` searches every dwell of a long block.** It pushed a
    block into its searcher once, with room for one dwell's list, so the
    search stopped after the first dwell that reported anything; it now
    resumes from `dp_acq_consumed()` until the block is taken (#2019).
