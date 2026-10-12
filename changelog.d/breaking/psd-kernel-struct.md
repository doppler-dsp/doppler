- **PSD's per-frame kernel is its own struct, and the frame calls take it.**
    `dp_psd_frame_power`/`_linear`/`_db` take `&psd->k`; `dp_psd_state_t`'s
    `fft`, `w`, `frame`, `spec`, `pwr`, `cg`, `s2`, `n`, `nfft` and
    `full_scale` move under `k` (#1894).
