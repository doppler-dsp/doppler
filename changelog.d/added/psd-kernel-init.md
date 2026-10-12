- **`dp_psd_kernel_init` / `dp_psd_kernel_free`: frames to spectra without a
    PSD.** The window, transform and dBFS reference, none of the averaging; a
    refusal returns `DP_ERR_INVALID` with nothing allocated (#1894).
