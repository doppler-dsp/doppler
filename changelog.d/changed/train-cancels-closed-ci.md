- **`make train` cancels the CI of the PRs it closes.** The re-vendored
    `scripts/train.py` (canonical standard, just-buildit.github.io#200)
    cancels each closed constituent's queued and in-progress runs, which
    gate nothing once the train exists and held capped macOS runners ahead
    of a release (just-makeit v0.120.2 waited 1.5 h on them, 2026-11-06).
