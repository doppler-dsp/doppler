- **PSD's four readouts take no `count`.** `psd_db`, `psd_dbhz`,
    `power_twosided` and `power_onesided` are sized by the object, and the
    kernels ignored the count they were given: `power_onesided(5)` returned
    all nfft/2+1 bins. Python now takes only `out=`, so passing a count raises
    `TypeError`. In C, the second parameter (`cap`/`n`) is gone:
    `dp_psd_power_twosided (state, out, max_out)` (#2184, jm gh-2233).
