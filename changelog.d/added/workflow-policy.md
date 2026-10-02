- **How We Work: one policy from branch to release.**
    `docs/dev/workflow.md` covers how to shape a change, prove it locally,
    merge it on green and up to date, stop the line on a red `main`, and
    keep a release a bump alone. It names the three lanes where CI runs
    less (a docs-only PR, `main` after a merge, a release PR) and the proof
    behind each. It covers how a change to the shared `standard.mk` lands,
    and which gate enforces each rule. Pre-commit is lint and formatting
    only, never skipped, and `make gates` is only for debugging a CI red.
    The policy moved there from `CONTRIBUTING.md` and `release.md`.
