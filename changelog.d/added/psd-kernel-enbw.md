- **PSD's kernel answers its window's ENBW.** `dp_psd_kernel_t.enbw` is the
    equivalent noise bandwidth in bins, computed once in
    `dp_psd_kernel_init`, so a kernel-only caller gets ENBW and RBW without
    a PSD; `dp_psd_state_t.enbw` is now a copy of it (#2211).
