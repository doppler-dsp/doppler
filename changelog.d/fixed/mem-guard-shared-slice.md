- **A docs build and a parallel test run can no longer take the dev VM down
    together.** `scripts/mem-guard.sh` now holds every guarded command to one
    shared systemd slice ceiling instead of one per command, and the docs
    build and every xdist pytest run go through it. `make mem-guard-check`
    (in `make lint`) fails on a heavy command that does not.
