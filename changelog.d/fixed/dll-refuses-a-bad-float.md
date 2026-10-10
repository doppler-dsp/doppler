- **The DSSS receivers, `Dll`, `Despreader` and the loops on the shared
    loop filter (`LoopFilter`, `Costas`, `SymbolSync`, `CarrierMpsk`,
    `CarrierNda`) raise a `ValueError` naming a bad argument, instead of
    crashing, aborting or running NaN gains.** Changed: `MemoryError` becomes
    `ValueError`; a loop's `configure` raises and its `bn` setter refuses
    (raising only after just-buildit/just-makeit#2182); `set_symbol_period`
    refuses `-inf` and a period past 2^20 partials; a coupled
    `carrier_freq_hz` must be 0 or above half the sample rate (#2103).
