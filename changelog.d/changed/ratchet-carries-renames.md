- **A ratchet line can carry a rename, and nothing else.** On the
    state-roundtrip and chunk-invariance ratchets, a new line ending
    `| replaces: <key> ...` takes over the keys it names. It counts as
    not ADDED only when every replaced key was on the merge base's list,
    every one is gone, and none backs a second new line, so a rename can
    merge entries but never grow the list. Any other new line still fails as
    ADDED. Found in #2752 step 3.2, where `Corr` and `Corr2D` become one
    object.
