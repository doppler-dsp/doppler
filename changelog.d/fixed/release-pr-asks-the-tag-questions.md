- **`make release-pr` now runs every tree-only check `make ship` will run,
    at a point where each can still see what it checks.** Its size check ran
    after assembly and measured an empty section ("0 entries" for v0.52.0,
    #1400), and the assembled CHANGELOG was rewritten by the mdformat hook on
    four releases running, rejecting the release commit.
