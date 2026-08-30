- **Re-vendored `standard.mk`, which now carries `gates-home-check`.** Upstream
    added the converse of `gates-check`: a target sitting in `GATES_DEPS` that
    no workflow runs used to pass without a word, so the repo could declare a
    gate that guarded no pull request — the failure doppler recorded twice and
    then found in `test-ubsan`/`test-tsan`, which had run on no PR ever. The
    vendored copy being behind failed `standard-check` on every branch.
    doppler passes the new gate: 26 gates, all with an execution home.
