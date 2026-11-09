- **wfm's allocation sizes go through the checked helpers.** Every
    inline `n * sizeof` product in the wfm composer, Plan, JSON, reader,
    writer and synth now calls `dp_mallocn`/`dp_xmallocn`/`dp_reallocn`/
    `dp_xreallocn`, so a count that wraps is refused or aborts instead
    of returning a short buffer. The two synth setters this reached are
    under Fixed.
