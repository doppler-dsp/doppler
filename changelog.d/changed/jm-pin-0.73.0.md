- **just-makeit pin 0.71.1 → 0.73.0.** It carries
    [just-makeit#1224](https://github.com/just-buildit/just-makeit/issues/1224),
    filed from here — an `init_param` can now name another generated class
    (`object = "frame.FrameDesc"`) instead of spelling the capsule string at
    both ends — plus gh-1219, which matters here: the impl provenance marker
    was emitted as one 132-column line into a `native/inc/**` header that
    `c_format_command` excludes, so a project at this repo's 79 columns could
    alternate between clean and STALE forever.
