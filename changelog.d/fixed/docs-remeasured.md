- **The docs' measured tables are checked, not restated.** The acquisition
    sizing tables (`guide/acquisition.md`, `guide/dsss-acquisition.md`), the
    RateSync EVM sweep and the DDC split seam are now `>>>` examples or
    asserts the doc gate runs, so a value that drifts fails the build; each
    re-measured value changed (e.g. 52 dB-Hz now sizes 8 Doppler bins of
    4125 Hz). The DDC architecture table is labeled as the 2026-05-17 study
    it is, pointing at the release benchmarks for the shipped DDC (#2585).
