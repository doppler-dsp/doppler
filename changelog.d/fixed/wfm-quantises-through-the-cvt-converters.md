- **`wfm`'s wire quantisers now call doppler's own converters, recovering
    6.1 dB on every integer capture.** The NATS sink, the file writer and the
    reader each carried a private float↔int copy that truncated toward zero
    where every `cvt` converter rounds, and used 2^(N-1)-1 as full scale where
    `cvt` uses 2^(N-1) — so an 8-bit capture sat at −46.9 dBFS instead of
    −53.0, and `wfm.Reader` disagreed with `cvt.I16ToF32` on 2.3% of int16
    codes ([#1117](https://github.com/doppler-dsp/doppler/issues/1117)).
    **Integer wire bytes change**; `cf32`/`cf64` are untouched.
