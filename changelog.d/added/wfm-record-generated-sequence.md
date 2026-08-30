- **A generated sequence survives `--record` → `--from-file`.** A PN or Gold
    sequence has no bit array, and `add_bit_string` writes nothing without one
    — so the field vanished from the record and `--from-file` rebuilt an
    *unframed* waveform at exit 0. It is now recorded under a `_gen` key
    beside the literal it replaces, which is the point of the generated kinds:
    six numbers reproduce a million-symbol sync word. Masks are hex strings,
    since a JSON number is a double and a 64-bit polynomial would not survive
    as one. Both spellings of one field, an unknown `kind`, or a bad register
    width are **refused**, not ignored. The DSSS chip builders take raw
    arrays, so a generated spreading code is now *expanded* before it reaches
    them: read through as a pointer it dereferenced NULL, and the standalone
    `Synth` face refused it outright for having no array — the one face a
    record restores through.
    Step 3 of [#762](https://github.com/doppler-dsp/doppler/issues/762).
