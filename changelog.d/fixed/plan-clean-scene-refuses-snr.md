- **A `Plan` over a scene with no noise refuses an `snr`** (#1695).
    `plan.at(snr)` and `render(snr=…)` returned the clean signal at every SNR,
    so a BER sweep over one read a perfect receiver. They now raise
    `ValueError` naming the fix; C gets `dp_wfm_plan_check_snr()` and a 0
    return.
