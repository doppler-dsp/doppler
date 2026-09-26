- **Groundwork for the `dp_` symbol prefix (#1545):** the test/validation
    tree no longer declares names the prefix will derive. The four
    `dp_ber_*` forwarding shims in `native/tests/dp_ber_test.h` are gone
    (callers use `ber_*`), four private `crc16` copies call `dp_crc16_ccitt`,
    and a private `rrc_taps` calls `wfm_rrc_taps`.
