- **The issue tier map is reconciled against the live list, daily.**
    `make issues-check` fails on an open issue with no tier or a tier row
    for a closed one, and `.github/workflows/issues.yml` runs it on a
    schedule rather than on pull requests, since filing an issue is not a
    diff. `make issues` now drops a closed issue's row itself and refuses
    only on an untiered one, so the fix for a red run is always "tier the
    new issues, run `make issues`". The map is reconciled too: 20 closed
    rows dropped, 71 open issues tiered, each with a one-line `why`.
    `GATES_CI_EXTRA` is held by `make gates-extra-home-check`. See
    `docs/dev/issues.md` (#1716).
