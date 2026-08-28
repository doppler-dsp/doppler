- **A 1500-line stale copy of `wfmgen.c` was sitting in the repo root**, under
    the name `fm_stream_sink_close (self->h);,+25p` — the residue of a quoting
    slip in a `sed -n '/…/,+25p'`, committed by an unrelated change and already
    drifting from the real file. Deleted, and gated: a tracked path must now be
    a name a person could type (`[A-Za-z0-9._/-]`), which is the one property
    that makes this class findable. Every other gate is keyed on a suffix or a
    directory, and this file had neither.
