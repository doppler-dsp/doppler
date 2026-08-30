- **Re-vendored `standard.mk` again.** Upstream widened what
    `GATES_LOCAL_ONLY` documents — an aggregate target whose work already gates
    a merge under other names now qualifies, alongside the original "cannot run
    on a runner" case. Comment-only; `gates-check` still covers all 33 CI
    make-targets and `gates-home-check` still finds 26 gates with an execution
    home. The vendored copy being behind failed `standard-check` on every
    branch, which is why this is its own change.
