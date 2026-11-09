- **wfm's allocation sizes go through the checked helpers.** All 31
    inline `n * sizeof` products in the wfm composer, Plan, JSON, reader,
    writer and synth now call `dp_mallocn`/`dp_xmallocn`/`dp_xreallocn`
    (or `dp_size_mul` before a plain `realloc`), so a count that wraps is
    refused or aborts instead of returning a short buffer.
    `dp_wfm_synth_set_symbols()` and `dp_wfm_synth_set_rrc()` now refuse
    such a count with -1; they used to copy past a wrapped 8- or 4-byte
    block.
