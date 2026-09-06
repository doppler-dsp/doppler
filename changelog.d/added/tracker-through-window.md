- **`validate_tracker_through_window`** (`make validate-c`, its `--check` in
    the C suite): the hand-off receiver, seeded by the searcher, through ten
    frames of the windowed waveform (design §12.9) — code lock never drops
    in a 167 ms pure-code window at either C/N0, symbol lock coasts it and
    needs no pull-in, and the release never fires. Under SPEC's Doppler
    ramp the same harness found the chain failing to pull in from the
    searcher's seed at the floor (#1249).
