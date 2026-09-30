- **The data source, in C (`wfm/wfm_data.h`)**
    ([#1619](https://github.com/doppler-dsp/doppler/issues/1619)). A
    `data:LEN` payload's bits come from a finite Field, a `pn:0` stream, a
    file or a pipe, `LEN` at a time: one draw per frame, the last chunk
    padded from `--fill`, and a pipe's pause reported as "nothing yet" for
    an idle frame. It refuses before the first sample what it can decide
    then, and it hashes a file as it reads it. Not wired to a face yet.
