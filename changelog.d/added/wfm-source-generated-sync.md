- **A source's sequence KIND now reaches the wire — a PN or Gold sync is
    finally spellable.** The frame layer has always materialised all four
    `wfm_seq_t` kinds, but `wfm_source_describe_frame` rebuilt every field as
    a fresh `WFM_SEQ_LITERAL`, discarding the kind one call before the
    descriptor could see it. It passes the caller's sequence through now, and
    `wfm_source_has_frame` tests `sync.len` rather than `sync.bits`: a
    generated sequence has no array, so a PN sync read as *unframed* and the
    payload went out bare. Pinned against `pn_generate` of the same three
    numbers — external truth, not a round trip.
    Step 2 of [#762](https://github.com/doppler-dsp/doppler/issues/762).
