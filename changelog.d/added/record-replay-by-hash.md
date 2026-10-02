- **A record replays its data by hash.** `--record` and SigMF now carry
    what each data source sent (`"data_sent"`; `wfmgen:frames`,
    `wfmgen:idle_frames`, `wfmgen:pad_bits`, and a file's or stdin's
    `dp_hash64`). A replay refuses a changed file, naming both hashes, and
    a record of stdin replays only with `--data-from-file FILE` given again
    ([#1619](https://github.com/doppler-dsp/doppler/issues/1619)).
