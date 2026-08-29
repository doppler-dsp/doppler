- **just-makeit pin 0.70.1 → 0.71.0.** Brings `c_ptr` / `c_len`
    (just-buildit/just-makeit#1184, filed from here): a composer source's
    `bytes` field can name its own C storage instead of being hardcoded to
    `src.<name>` / `src.n_<name>`, which is what lets `wfm_source_t` carry
    `wfm_seq_t` rather than flattening ten generator parameters per sequence
    — the blocker on [#762](https://github.com/doppler-dsp/doppler/issues/762).
    Zero codegen drift once `[acq]`'s manifest `doc` was dropped: it silently
    disagreed with `acq_create_continuous`'s header `@brief` and, now that jm
    delivers a manifest doc to the stub, would have replaced the specific text
    with a generic one. The header stays the single source for that prose.
