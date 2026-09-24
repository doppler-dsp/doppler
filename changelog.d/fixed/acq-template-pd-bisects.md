- **`validate_acq_template_pd` fits its sweep budget again.** It found each
    design point by a quarter-dB scan that built a burst engine per step,
    and sizing on the burst (#1498) made each build dearer: 2.4 s on CI
    against 1.1 s. Both scans are now bisected on the same grid, with
    byte-identical output.
