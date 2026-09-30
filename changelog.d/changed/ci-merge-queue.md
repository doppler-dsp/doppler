- **CI is ready for the merge queue.** A pull request now runs only the fast
    gates, with Python on 3.9 alone. The full matrix runs on `merge_group`
    and on push. `CI passed` accepts heavy jobs skipped on a PR and refuses
    them skipped anywhere else. `make ci-aggregator-check` holds the heavy
    set to one declaration. See `docs/dev/ci.md` (#1710).
