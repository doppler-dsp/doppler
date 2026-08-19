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
    invocation. All seven provisioning call sites also gain a step-level
    `timeout-minutes: 25`, the backstop for a shell with no `timeout(1)` and
    the half that cannot be bypassed. `timeout-minutes` is **not supported on
    composite-action steps**, so unlike `setup-uv` this cannot be folded into a
    single action.

    Sized from measurement, not feel: healthy runs took **16–179s across 9
    samples** (median 18), so the 300s per-attempt deadline is ~1.7x the worst
    observed.

    **A deadline expiry is terminal and is not retried**, which the fix's own
    first live run is what established. Attempt 1 hit the 300s deadline inside
    `apt-get` and attempts 2 and 3 then failed in seconds with `Could not get   lock /var/lib/dpkg/lock-frontend. It is held by process 2429 (apt-get)`.
    The killed apt-get **survived**: jbx runs it under `sudo`, so it is
    root-owned and an unprivileged process-group kill gets `EPERM`. It then
    holds the lock forever, because the reason it was killed is that it hung.
    Retrying past that point is not merely wasteful — it is guaranteed to fail
    and it buries the real cause under two lock errors. Ordinary failures (a
    curl exiting non-zero, a transient resolver error) leave no lock and *are*
    still retried.

    **The stall is a mirror going quiet mid-download.** The log shows
    `azure.archive.ubuntu.com` producing nothing for 229 seconds partway
    through a 114 MB fetch, on the same package (`cmake-data`) across separate
    runs — a broken path, not random noise. `make apt-stall-config` sets
    `Acquire::http::Timeout "30"` / `Acquire::Retries "3"` before provisioning
    (Linux-only, best-effort, a no-op without apt or sudo). **Measured, that
    did not stop it**: apt carried the config and still sat nine minutes on the
    same package, so the deadline is what bounds this, not apt. The setting
    stays because it costs nothing and covers stall modes apt *can* see, but it
    is not the thing that works here.

    **What makes the retry viable is reclaiming the lock.** The holder is
    root-owned, and a CI runner gives us root — so on a deadline expiry the
    script kills it, removes the dpkg/apt lock files, runs `dpkg --configure   -a`, and retries, quite possibly against a different mirror node. That is
    gated on `CI` *and* sudo: clearing dpkg locks is reasonable for a
    disposable VM and unreasonable for a laptop, so off a runner a deadline
    expiry stays terminal.

    The deadline moves to **600s**: the 9 samples it was first sized from were
    all fast ones, so ~3.4x the worst observed is the honest margin, and a true
    hang is still bounded at ten minutes against the 360 it used to get.

    **The retry budget has to fit the step ceiling, and the first version did
    not.** 600s x 3 tries is 30 minutes against the `timeout-minutes: 15`
    those sites first carried, so the
    retry — which the reclaim had just made work, apt restarting cleanly with
    no lock error — was killed mid-download four minutes later. Now 2 tries
    against a 25-minute ceiling (~20.2 min of budget), and `make   deps-budget-check` derives both sides from the real numbers and fails if
    they ever stop fitting. Sabotage-proven: it reddens on the exact 600x3
    against-15 pairing that shipped, and refuses rather than passing silently
    if it can find no `timeout-minutes:` to check against.

    **The deadline bounds provisioning; a hang elsewhere was still
    unbounded.** Measured against a second incident while this branch was
    open: in PR #880 provisioning succeeded and then `make test-python` hung
    **70 minutes** inside `Test with coverage`, reporting `pending`
    throughout — the same symptom, at a step none of the fix's commits touch.
    It had to be cancelled and re-run by hand, which is the manual recovery
    the deadline exists to remove. So **all 25 jobs now carry a job-level
    ceiling**, a different mechanism that does not replace the deadline: a
    ceiling only kills, where the deadline kills *and retries*, so
    provisioning keeps the pairing that recovers. Sized from **180 successful
    jobs** — `coverage` to 90 minutes against a legitimate 54.7-minute run,
    every other job to 45 against a 30.0-minute worst case. `docs.yml` is 45
    rather than the 20 its 3-minute work suggests, and `deps-budget-check` is
    what caught that: at 20 the 1210s provisioning budget no longer fits,
    because a *job* ceiling bounds the provisioning step inside it just as a
    step ceiling does. That is the gate doing exactly what it was built for,
    one commit after it was written, on a value it was not written to police.

    Also measured on that run: **GitHub's macOS runner has neither
    `timeout(1)` nor `gtimeout`**, so the script carries a POSIX watchdog
    fallback with the same contract rather than leaving a whole platform on the
    workflow ceiling alone.

    Every path verified by sabotage: the deadline fires (`rc=124`, one attempt,
    no retry), a plain failure propagates its own code and still retries, a run
    that recovers on attempt 2 exits 0, and the missing-`timeout(1)` case
    announces itself rather than degrading silently.
