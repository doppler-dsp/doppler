- **The halfband decimators refuse a forged ring head.** `hbdecim`,
    `hbdecim_r2c` and `hbdecim_q15` check their ring heads, pending flag
    and (r2c) parity before writing anything. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
