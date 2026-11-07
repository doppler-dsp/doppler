- **A public C name removed on a branch now needs a `.retired-names`
    row** (#2905): `make lint-retired-symbols` fails when a symbol in
    `cmake/public-symbols.txt` or a header under `native/inc/doppler/` is
    gone since the merge base with `origin/main` and no row's pattern
    matches it, so `check_retired_names.py` then finds every caller still
    using the old name.
