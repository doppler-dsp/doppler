- **`wfmgen` can finally ask for a generated sequence.** `wfm_seq_t` has
    carried PN/Gold/Dotted since it existed and the frame layer materialised
    all three, but every route in flattened the kind to LITERAL, so no command
    line could spell one. `--acq-code-gen`, `--data-code-gen` and `--sync-gen`
    take `KIND:LEN[:...]` — one flag per sequence with the kind as data, the
    same vocabulary the record uses. A 1023-chip sync word is six numbers now
    instead of a 1023-character string. Giving a field both spellings is
    **refused in either order**: accepting it wrote a record that this tool's
    own `--from-file` could not read.
    Step 4 of [#762](https://github.com/doppler-dsp/doppler/issues/762).
