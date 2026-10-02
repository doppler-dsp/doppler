- **`main` no longer re-runs the matrix a PR just ran, and runs it
    nightly instead.** On a push, `changes` asks `make ci-tree-tested`
    whether the landed tree already passed `CI passed` as a PR head that
    contained the previous tip. `protect-main` requires PRs to be up to
    date, so that is every merge, and the push run skips to the cheap
    gates. A nightly `schedule` run never skips, so environment drift goes
    red on `main`. `standard.mk` is re-vendored for the target.
