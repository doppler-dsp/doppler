- **`Synth.set_dsss_window(code_only_epochs, frame_epochs)` — the continuous
    DSSS stream gains a frame.** Every `frame_epochs` code periods open with
    `code_only_epochs` of the pure code and no data; the data section's symbol
    clock is aligned to its first chip and the payload runs on across frames.
    `frame_epochs=0` is the stream exactly as before. The multi-emitter
    waveform's 500-in-5500 window, and what the searcher's coherent depth is
    measured against (design §12 step 10).
