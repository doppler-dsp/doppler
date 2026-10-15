- **The burst engine's false-alarm rows run in CI at enough dwells to see a
    doubled rate.** `validate_acq_rate_pd_pfa` holds n = 1, 3 and auto over
    100,000 dwells each (#2324), and `validate_capture_dwell_pd`'s full run
    now asserts its bounds at every depth (#1566).
