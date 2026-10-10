- **`AsyncDsssPool` searches every dwell of a long block.** It pushed each
    block into its searcher once, with room for one dwell's list
    (`max_peaks`), so once a block made more than `max_peaks` hits the rest
    of it went unsearched and the emitters in it were missed. It now resumes
    from `dp_acq_consumed()` until the block is taken (#2019).
