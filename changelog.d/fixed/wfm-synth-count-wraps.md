- **Two heap overruns in `wfm_synth`.** `dp_wfm_synth_set_symbols()`
    and `dp_wfm_synth_set_rrc()` sized their copy as `n * sizeof elem`
    from the caller's count, so a count past `SIZE_MAX / sizeof elem`
    wrapped to an 8- or 4-byte block that the copy loop then wrote past.
    Both now refuse that count with -1, and a refused `set_symbols`
    leaves the stream it had.
