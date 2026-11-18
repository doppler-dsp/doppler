- **`RateSync` and `SymbolSync` locks wait two decisions to declare and six
    to drop.** The counts derive from the threshold's sizing rates at a
    per-decision budget of 1e-6, where both declared on one block
    ([#3133](https://github.com/doppler-dsp/doppler/issues/3133)).
