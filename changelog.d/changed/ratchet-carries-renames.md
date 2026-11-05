- **A shrink-only list can carry a rename, and nothing else.** The
    state-roundtrip and chunk-invariance ratchets (`| replaces: <key> ...`)
    and the mutator-state list (a trailing `replaces: <key>`) let a new
    entry take over the keys it names, under one rule, `carry_refusals`:

    - every replaced key was on the merge base's list;
    - every replaced key is gone at head;
    - no replaced key backs a second new entry;
    - no replaced key is still live.

    "Live" is each list's own: an object's `_set_state`, or a mutator's C
    function. The mutator list adds a fifth condition: the verdict is
    unchanged. So a rename can merge entries, but never grow a list, trade
    a fixed entry for a new one, or move a row into another verdict. Any
    other new entry still fails as ADDED. Found in #2752 step 3.2, where
    `Corr` and `Corr2D` become one object.
