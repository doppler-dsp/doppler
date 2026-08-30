- **The waveform enum name tables have one C home, gated.** `just-makeit.toml`
    has claimed since gh-285 to be their single source; measured, that was true
    of the Python binding only, while `wfmgen.c` declared twelve of its own and
    `wfm_json.c` seven. One pair had already drifted into OPPOSITE orders — the
    `--data` sources — harmless only because one file compared its lookup
    result where the other assigned it. All nineteen now live in
    `native/inc/wfm/wfm_names.h`, each annotated with the `[[enum]]` that owns
    it, and `make lint-wfm-enum-tables` holds the header, the manifest and the
    C enums to each other. List order IS the enum value, so a copy that drifts
    maps a flag to the wrong waveform. Design: `docs/design/waveform-enum-ssot.md`
