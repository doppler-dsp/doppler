- **A synth's state blob carries its DSSS window** (`set_dsss_window`'s
    two counts), so a synth restored without the call resumes the framed
    stream, and a blob whose window breaks the setter's rule is refused. The
    blob is version 5; a version-4 blob, and any composite holding one, is
    refused rather than resumed without its window (#2022).
