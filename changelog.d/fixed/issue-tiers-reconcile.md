- **The issue tier map is reconciled against the live list, daily.**
    `make issues-check` fails on an open issue with no tier or a tier row
    for a closed one, and `.github/workflows/issues.yml` runs it on a
    schedule rather than on pull requests, since filing an issue is not a
    diff. The map is reconciled too: 20 closed rows dropped, 71 open issues
    tiered, each with a one-line `why`. `GATES_CI_EXTRA` is now held by
    `make gates-extra-home-check`. See `docs/dev/issues.md` (#1716).
