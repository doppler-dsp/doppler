- **CI's `changes` job is the org's vendored workflow.** `ci.yml` calls
    canonical's `.github/workflows/changes.yml` instead of its own inline
    classifier, so a version bump, a docs-only diff and a merged tree its PR
    already tested are judged as in every just-buildit repo. `pin`,
    `pre-commit` and `manifest-drift` still run on every tree, declared in
    `CI_ALWAYS_RUN_JOBS`; the Python matrix and its primary leg come from a
    new `pythons` job. `standard.mk` is no longer pinned
    ([#1809](https://github.com/doppler-dsp/doppler/issues/1809)).
