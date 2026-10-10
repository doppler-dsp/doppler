- **`deploy/README.md` no longer says `stream_tool` catches a dropped or
    duplicated frame.** It verifies each frame's PN payload against that
    frame's own `sequence`, so it catches corruption, not a missing or
    repeated frame, and the README now states that limit (#2017).
