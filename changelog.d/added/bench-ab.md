- **`make bench-ab A=<ref> B=<ref>` compares two commits.** It builds both at
    one set of flags, alternates them K times, and reports per row each
    side's min of the best rounds, its spread, the ratio and a verdict. A row
    ties within the larger spread, and the run fails on a slower or missing
    row (#2759). `make bench-interleaved` stays the release tool.
