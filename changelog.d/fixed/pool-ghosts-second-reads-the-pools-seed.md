- **The pool's ghost harness no longer fails a draw the pool got right.**
    Its SECOND arm waited for an idle receiver the pool's own search had
    already put on the second emitter, and failed 20 of 200 full-sweep draws
    as "receiver 0 never tracked A" while both receivers held their own
    emitter. Those draws are now scored, and `--check` pins one
    ([#3267](https://github.com/doppler-dsp/doppler/issues/3267)).
