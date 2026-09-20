- **The tracked-paths gate now comes from canonical instead of a copy here.**
    `scripts/check-tracked-paths.sh` is deleted and `LINT_tracked-paths` is
    gone from `LINT_TOOLS`; the same two rules — every tracked name is one a
    person could type, and no two names differ only in case — arrive with
    `standard.mk` as `make tracked-paths-check`, and the pre-commit hook
    dispatches there. The rule was written here after `test_Resampler.py` was
    found sitting beside `test_resampler.py` on a Windows checkout, and it
    turned out to be a rule every repo needs; a second copy of a check is the
    thing that drifts. `test_tracked_paths_gate.py` still exercises it, now by
    seeding a throwaway repo with the vendored `standard.mk`.
