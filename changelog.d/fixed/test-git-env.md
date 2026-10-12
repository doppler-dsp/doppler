- **The test suite no longer rewrites your git index when run from a rebase
    exec or a hook.** `git rebase -x` exports `GIT_DIR`, and the gate tests'
    scratch repos obeyed it (3,771 staged deletions in one replay); the root
    `conftest.py` now clears git's redirect variables in every pytest process
    (#2212).
