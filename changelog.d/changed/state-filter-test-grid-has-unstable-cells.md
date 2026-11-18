- **`StateFilter`: the stability grid now holds unstable loops, and the
    state-restore tolerance is pinned.** The delay-and-gain grid that judged
    `stable()` against a simulation had no unstable cell (so a `stable()` that
    always said 1 passed it); it has twelve now. The 1e-9 gain tolerance of the
    restore check is held a decade either side, and two header claims that no
    test held are reworded to what is held.
