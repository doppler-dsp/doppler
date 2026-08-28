- **`make pr-watch PR=<n>`** — the PR-check watcher, vendored from canonical
    and held there by `standard-check`, instead of hand-copied per repo. It
    reports; it never merges. `gh pr merge --auto` remains the gate.
