- **`Synth.set_dsss_window(code_only_symbols, frame_symbols)` — the
    continuous DSSS stream gains a frame.** Of every `frame_symbols` symbols
    on the data clock, the first `code_only_symbols` carry the pure code and
    no data, the rest the payload, running on across frames. The symbol clock
    free-runs through the window: the chip and data clocks have no fixed
    relation, and a frame edge falls at no particular chip phase.
    `frame_symbols=0` is the stream exactly as before. The multi-emitter
    waveform's 450-in-4950 window, and what the searcher's coherent depth is
    measured against (design §12 step 10).
