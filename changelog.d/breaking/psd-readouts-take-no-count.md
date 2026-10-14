- **PSD's four readouts take no `count`: delete the `count` argument.**
    `psd_db`, `psd_dbhz`, `power_twosided` and `power_onesided` take only
    `out=`; in C the ignored second parameter is gone (#2184).
