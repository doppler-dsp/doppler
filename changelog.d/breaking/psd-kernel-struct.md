- **PSD's per-frame kernel is its own struct, and the frame calls take it.**
    `dp_psd_frame_power`/`_linear`/`_db` take `&psd->k`; passing the PSD
    itself is unsupported, even where a compiler still accepts it.
    `dp_psd_state_t`'s `fft`, `w`, `frame`, `spec`, `pwr`, `cg`, `s2`, `n`,
    `nfft` and `full_scale` move under `k`, so code holding a PSD (carrier_acq,
    tonemeas, imdmeas, nprmeas and specan do) reads `psd->k.nfft` (#1894).
