- **A generated sequence survives `--record` → `--from-file`.** A PN or Gold
    sequence has no bit array, so the field vanished from the record and
    `--from-file` rebuilt an *unframed* waveform at exit 0. It is recorded
    under a `_gen` key now — six numbers reproduce a million-symbol sync word
    — and the DSSS chip path expands one instead of dereferencing its NULL or
    refusing it for having no array. Both spellings of one field, an unknown
    `kind`, or a bad register width are **refused**, not ignored.
    Step 3 of [#762](https://github.com/doppler-dsp/doppler/issues/762).
