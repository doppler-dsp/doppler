- **`scripts/check_wfmgen_flag_docs.py` — every flag `wfmgen` accepts is
    named in a `docs/guide/wfmgen/` page, now 58 of 58.** A review had
    counted 50 of 56 and #1044 closed the gap by hand; nothing kept it
    closed. On its first run this found `--randomize`, the American spelling
    the parser accepts and eleven pages had never mentioned. Aliases count,
    pages are discovered by glob, and there is no allow-list.
    [#1055](https://github.com/doppler-dsp/doppler/pull/1055)
