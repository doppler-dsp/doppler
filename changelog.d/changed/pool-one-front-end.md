- **`AsyncDsssPool` resamples the stream once.** When a code period is
    not a whole number of input samples, the pool resamples to the
    search's 2,048 samples a period and builds the search and every
    receiver at that rate, so none resamples again; `status()` still
    reports in the caller's samples.
