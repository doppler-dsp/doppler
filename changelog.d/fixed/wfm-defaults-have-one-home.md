- **A wfm default is declared once and rendered for C, so the CLI, a C caller
    and Python cannot disagree.** `wfmgen.c` restated the eleven values as a
    struct literal whose own comment said it *mirrored* the manifest, with no
    gate between them — and `modulation`/`crc` were restated as enum
    **indices**, so prepending an entry to `[[enum]] bitmod` or `[[enum]] crc`
    would have changed a default with nothing failing.
    `scripts/gen_wfm_defaults.py` now renders `wfm_defaults.h` from the
    manifest, resolving an enum default through the field's **own** declared
    `enum` (`"bpsk"` sits in two enums, so search-by-value picks the wrong
    one). Closes
    [#1142](https://github.com/doppler-dsp/doppler/issues/1142).
