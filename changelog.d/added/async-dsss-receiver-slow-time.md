- **`AsyncDsssReceiver` searches in slow time, on a shared block grid, at
    a thread count you set.** New create parameters `code_only_s` and
    `doppler_rate` size the search's coherent depth (read back as
    `coherent_bins`; SPEC's 450-symbol window at 500 Hz/s runs D = 90), so
    at 32 dB-Hz it finds a signal inside one code-only window that the
    one-epoch search cannot. `set_threads()`/`threads` forward the engine's
    pool. A restarted search now begins on the next multiple of
    `D * sf * spc` input samples since create (one code epoch at the
    default D = 1); the state blob is v12.
