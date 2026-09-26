- **`cvt.U8ToF32` reads RTL-SDR `cu8` I/Q** — unsigned, offset-binary bytes
    centred on 127.5, which `I8ToF32` misreads as signed (its docs named
    RTL-SDR; they now name HackRF's `cs8`). `mode="shift"` (default) is exact
    and fast, `(x - 128)/128`, reading 0.5/128 low; `mode="midpoint"` is the
    unbiased, symmetric `(x - 127.5)/127.5`.
