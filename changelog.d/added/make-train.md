- **`make train PRS="<n> <n>"`** lands two or more ready PRs as one PR,
    so `protect-main`'s strict check costs one CI run instead of one per
    PR. It is now the preferred way to land PRs; see
    [workflow §4](docs/dev/workflow.md).
