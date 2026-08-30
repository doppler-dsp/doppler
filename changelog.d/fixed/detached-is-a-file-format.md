- **`wfmgen --detached` refuses what it cannot honour, instead of dropping
    it.** It selects BLUE's detached-header format (`<out>.hdr` + `<out>.det`),
    and only one destination honoured it — so `--realtime` was silently
    ignored, and `--detached` itself was discarded for any other `--file-type`
    or a `nats://` output. All four now exit 2 with a specific message.
    Refusing rather than pacing settles gh-725: the `.hdr` carries the final
    sample count and cannot be written until the drain ends, so nothing can
    read the pair while it is paced. The tool's own `--help` had called it
    "a detached background process" and filed it under REAL-TIME, which is
    what made `--realtime` look applicable; it now sits under OUTPUT.
