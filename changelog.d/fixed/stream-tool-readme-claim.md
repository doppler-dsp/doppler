- **`stream_tool` compares Q as well as I, and it and `deploy/README.md`
    say what it catches.** It verifies each frame's PN payload, seeded by
    `sequence % 127`, against that frame's own `sequence`, so it catches a
    corrupted payload, not a missing, repeated or transport-rejected frame
    (#2017).
