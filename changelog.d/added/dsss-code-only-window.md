- **Continuous DSSS takes its per-frame code-only window on every face.**
    `code_only_symbols` of every `frame_symbols` carry the code alone, so a
    searcher can integrate the pure code; the data pauses through each window.
    It is a field of the public `Synth`, a `Segment`, a scene and a `Plan`, and
    `--code-only-symbols`/`--frame-symbols` on the CLI, recorded in the SigMF
    sidecar. Before, only the private `_SynthEngine.set_dsss_window` had it
    (#1294).
