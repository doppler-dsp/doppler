- **`doppler.detection` gains `det_pfa_cell`, `det_pd_cfar`,
    `det_cn0_to_snr` and `det_snr_to_cn0`.** The search-level Pfa split,
    the cell-averaging CFAR Pd model and the C/N0 conversions, previously
    private to `acq` and `async_dsss_receiver`. The `ber` meter's lag
    search now uses the same Šidák split instead of Bonferroni's
    `pfa / n`, so its threshold drops by a hair.
