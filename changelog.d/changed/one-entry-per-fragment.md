- **A changelog fragment holds one entry.** The fragment lint
    (`make lint-changelog-fragments`) now refuses a second top-level `- `
    bullet in one file: the assembled list then differs from what mdformat
    writes, and v0.82.0's `make release-pr` stopped at its own commit on
    exactly that (#2993). An indented sub-bullet, or a list inside a code
    fence, is still one entry.
