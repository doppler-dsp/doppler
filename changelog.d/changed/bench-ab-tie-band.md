- **`make bench-ab`'s tie rule resists an outlier and states its floor.** A
    row's band is now the gap between its two best passes, since one slow
    pass widened a max − min band enough to hide a 50% regression. A tie is
    never under 1% of A's min, twice the bias a null run of one commit
    measured. It counts only a snapshot its own pass wrote, cleans up a
    failed build's worktree, and refuses a dirty `HEAD` (#2771).
