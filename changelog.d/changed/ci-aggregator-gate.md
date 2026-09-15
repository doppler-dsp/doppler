- **`make lint` now checks that every CI job gates a merge.** The
    `protect-main` ruleset requires a single status check, `CI passed`, so a
    job in `ci.yml` that is missing from the `ci-passed` job's `needs` could
    fail on every pull request and block nothing. `ci-aggregator-check` fails
    on a missing or unknown need, a renamed aggregator, or one that would be
    skipped instead of failing. `CONTRIBUTING.md`, `docs/dev/ci.md` and
    `docs/dev/release.md` now describe the ruleset as it is.
