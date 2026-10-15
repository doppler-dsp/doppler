- **A synth's state blob carries its DSSS window and is version 5; a blob
    saved by v0.71.0 or earlier is refused** (`set_state()` raises
    `ValueError`), as is any blob that embeds a synth's. Re-create the object.
    The window is `set_dsss_window`'s two counts, so a synth restored without
    that call resumes the framed stream, and a blob whose window breaks the
    setter's rule is refused (#2022; one setter of #2082, which stays
    open).
