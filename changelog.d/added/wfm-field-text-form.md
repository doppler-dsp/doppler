- **One reader and one writer for a Field's text form.**
    `dp_wfm_field_parse` reads the grammar every text face will share:
    `0101`, `0x1ACFFC1D`, `pn:LEN:REG[:SEED[:POLY]][:galois|fibonacci]`,
    `gold:…`, `dotted:LEN` and `*REPS`. `dp_wfm_field_format` writes its
    canonical form back, and `dp_wfm_field_bits` goes straight from text to
    bits. A refusal names its cause, and a number must be consumed whole, so
    `pn::10`, `12abc` and `010` are refused or read as written rather than
    guessed at (#853).
