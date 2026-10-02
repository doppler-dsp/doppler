- **Every pull request runs the full CI matrix, and there is no merge
    queue.** `ci.yml` has no `merge_group` trigger. `CI passed` grants a
    skip only to a version bump alone, and `make ci-aggregator-check`
    refuses any trace of a pull_request/queue split coming back. See
    `docs/dev/ci.md`.
