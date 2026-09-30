- **A dsss source missing a code says which one** (#1696). A frame with no
    `data_code`, a burst with neither a preamble nor a frame, and a
    continuous stream with no `data_code` are refused naming the code to
    give, on the CLI, in a scene, from `Synth` and from C
    (`dp_wfm_compose_create_why`). A preamble alone stays valid, and
    `waveforms.md` says so.
