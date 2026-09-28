- **`wfmgen --help` says what the flags do.** `--realtime-resync`
    re-anchors the clock whenever output falls behind, not at segment
    boundaries. `--interleave` runs before `--conv`, not last. `--type   bpsk/qpsk/pn` frame once `--payload-len` bounds their payload, rather
    than refusing framing outright. The same corrections reach the guide,
    and stale comments naming removed tools and transports are fixed (#1598).
