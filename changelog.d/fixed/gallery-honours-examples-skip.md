- **`make gallery` reads the same skip list the examples gate does, so release
    step 2 can pass again.** The target ran every script in `GALLERY_SCRIPTS`
    and exited 1 on the first failure. One of them,
    `mpsk_receiver_performance_demo.py`, has been in
    `src/doppler/examples/.examples-skip` since its Monte Carlo was found to be
    draw-dependent — the smoke gate deliberately does not run it — while
    `gallery` ran it anyway and failed on it.

    So the two disagreed about the same judgement, and `make gallery` could not
    pass on any machine. That is step 2 of
    [the release checklist](../dev/release.md), which is where it was found:
    preparing for a release runs straight into it.

    The list is now read rather than restated. A skipped script prints `SKIP`
    with the file it came from — a quietly absent panel is how the stale assets
    in [#780](https://github.com/doppler-dsp/doppler/issues/780) accumulated —
    and the PNG move tolerates a file a skipped script never produced.

    Proven by sabotage: removing the entry from `.examples-skip` puts the
    target back to red, and restoring it goes green.

- **`make gallery` no longer leaves four untracked files in the repo root.**
    The demos that write a capture leave it where they ran. `burst.blue` was
    cleaned up; the SigMF and BLUE pairs from the `wfm_io` / `wfm_write` demos
    (`probe.ci16`, `probe.ci16.sigmf-meta`, `scene.cf32`,
    `scene.cf32.sigmf-meta`) were not — none of them gitignored, which is how a
    build artifact gets committed by accident.

- **35 committed gallery assets were stale and are regenerated.** #780 counted
    21; it is 35 now, and the figures are deterministic (verified by
    regenerating one twice and comparing hashes), so the diff is real content
    rather than PNG churn. #780's remaining halves — the gallery has no gate,
    and one script is in no target — are untouched.
