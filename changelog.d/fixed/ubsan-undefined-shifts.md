- **Four undefined operations UBSan had been reporting to nobody.** A left
    shift of a negative value in all three `shl_*` kernels (`shl_q15`, `shl_q8`,
    `shl_i64` — half of every Q-format input is negative), and a
    `memcpy(dst, NULL, 0)` on the zero-length publish path in `stream_nats`.
    [#1026](https://github.com/doppler-dsp/doppler/issues/1026)
