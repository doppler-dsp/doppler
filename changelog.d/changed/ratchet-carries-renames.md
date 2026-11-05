- **A shrink-only list can carry a rename, and nothing else** (#2880). The
    state, chunk-invariance and mutator-state lists let a new entry take over
    the keys it names, under one rule (`carry_refusals`), so a rename never
    grows a list, trades a fixed entry for a new one, or changes a verdict.
