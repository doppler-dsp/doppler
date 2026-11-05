- **Short-frame benchmark rows for `Corr` and `CorrDetector`**, at the PN
    periods 31, 63 and 127 (#2752 step 3.1b):

    - `corr::execute[accumulate]` and `[dump]` at `[n=31]`, `[n=63]` and
        `[n=127]`;
    - `detector::push[frame=31]`, `[frame=63]` and `[frame=127]`.

    They time 64 calls a round, since one call is near the clock's
    granularity. Each checks what it timed, an m-sequence detected at its
    planted lag, or the bench fails. They are the baseline the one
    correlator must meet (its design, #2863, U1).
