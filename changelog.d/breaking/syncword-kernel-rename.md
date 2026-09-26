- **`dp_syncword_find` / `dp_syncword_pfa` are now `dp_syncword_search` /
    `dp_syncword_search_pfa`** (`doppler/dp_syncword.h`). Under the coming
    `dp_` symbol prefix (#1545), the `syncword` object's `find`/`pfa` methods
    take the old names, and two different C functions cannot share one. Same
    signatures and behaviour; `SyncFinder` in Python is unchanged.
