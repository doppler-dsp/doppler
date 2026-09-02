- **`Dll.set_symbol_period` now aids the code loop as well as the lock
    detector.** The discriminator runs on the symbol-aided window and the
    loop steers once per symbol, its filter re-timed so `bn` keeps its
    per-epoch meaning and the tracked rate is continuous across the switch.
    Measured against the per-epoch look-back at the operating point
    (`validate_dll_aid_jitter`,
    [design §12.5](docs/design/async-dsss-receiver.md)): pull-in 20%
    faster, jitter 0.8× above 45 dB-Hz and 1.3× at the 40 dB-Hz floor —
    hundredths of a chip either way. State blob version 9.
