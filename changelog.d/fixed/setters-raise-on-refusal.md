- **Nine writable properties raise `ValueError` on a value their C setter
    refuses, instead of ignoring it.** `AccTrace.alpha`, `AWGN.amplitude`,
    `Despreader.bn_carrier`/`bn_code` and `bn` on `CarrierMpsk`,
    `CarrierNda`, `Costas`, `Dll` and `SymbolSync` each already refused a
    value outside its domain in C and kept the old one. The Python setter
    dropped that status, so the assignment looked as if it had worked. The
    value still stays put; the assignment now says so
    ([#1987](https://github.com/doppler-dsp/doppler/issues/1987)).
