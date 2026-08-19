- **A hung `install-deps` can no longer block a PR for hours.** CI's dependency
    provisioning reaches the network three times — the jbx bootstrap
    `curl … get-jb.sh | bash`, then jbx's own apt/brew — and none of those calls
    carried a timeout. `get-jb.sh` itself makes three curls with neither
    `--retry` nor `--max-time`. So a stalled mirror did not *fail* the step, it
    **hung** it, until GitHub's 360-minute job limit, reporting `pending` the
    whole way where a reader cannot tell it from slow CI. Measured 2026-08-19:
    one commit hung **3h45m** and then **2h42m** at `Install system   dependencies`, blocking an otherwise-green PR, while a sibling branch passed
    the identical step ninety seconds later.

    **A retry alone would have been decoration, and that is the finding.** This
    repo already runs a retry-on-failure pattern
    (`.github/actions/setup-uv/action.yml`), and it keys on a step *exiting*
    non-zero — correct for the 2026-08-07 incident it was built for, where a
    fetch timed out and returned an error. A hung step never exits, so the same
    pattern bolted onto provisioning would never once have fired. The deadline
    is what converts the hang into a failure; the retry is what then recovers
    instead of merely failing faster. They ship together, deadline first.

    New `scripts/with-deadline.sh` carries both, reached through
    `make install-deps-ci` / `make install-docs-deps-ci` so the Makefile stays
    the one place that says *how* a tool runs — a developer keeps the plain
    target, CI reaches for the bounded one, and neither is a second copy of the
    invocation. All seven workflow call sites also gain `timeout-minutes: 15`,
    the backstop for a shell with no `timeout(1)` and the half that cannot be
    bypassed. `timeout-minutes` is **not supported on composite-action steps**,
    so unlike `setup-uv` this cannot be folded into a single action.

    Sized from measurement, not feel: healthy runs took **16–179s across 9
    samples** (median 18), so the 300s per-attempt deadline is ~1.7x the worst
    observed. Every path is verified — deadline fires (`rc=124`), a plain
    failure propagates its own code, a run that recovers on attempt 2 exits 0,
    the no-`timeout(1)` degradation announces itself and still retries, and a
    grandchild `curl`/`apt` is killed with its parent rather than leaking a
    dpkg lock that would make every retry fail.
