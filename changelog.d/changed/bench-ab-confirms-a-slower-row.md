- **`make bench-ab` holds a SLOWER row to a second run.** Rerun the pair
    with `CONFIRM=first.json`, the first run's `JSON=`: a row then fails
    only when it is SLOWER in both runs, and one flagged in a single run
    reads UNCONFIRMED and passes. An A/A run of one commit read 11 of 884
    rows SLOWER by chance, so one run could not pass identical code
    (#2984).
