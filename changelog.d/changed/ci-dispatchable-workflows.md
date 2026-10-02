- **Every self-triggered workflow can be started by hand.** `ci.yml`,
    `docs.yml` and `release.yml` take `workflow_dispatch`, because a run
    that fails at startup cannot be re-run. A dispatched release names an
    existing tag (`tag` input): every checkout builds it, `verify-ci` polls
    its commit, and the GitHub Release is created for it explicitly, never
    inferred from `github.ref`. A dispatched `docs.yml` on `main`
    redeploys the site. Pre-commit's four non-`make` hooks are declared in
    `HOOK_DISPATCH_EXEMPT` (just-makeit#1801).
