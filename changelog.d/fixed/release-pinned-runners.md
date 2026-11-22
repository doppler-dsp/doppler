- **A release no longer rides a floating runner image.** v0.84.0's x86_64
    PyPI smoke for Python 3.9 could not start: `ubuntu-latest` had rolled to
    26.04, where `setup-python` has no 3.9, after the wheels were already
    published (#3188). `release.yml` now names `ubuntu-24.04` (what v0.83.0 ran on) and
    `windows-2025-vs2026` (what `windows-latest` was on v0.84.0), and `make release-runners-check` (in `make lint`) refuses a `-latest`
    there. `ci.yml` stays on `-latest` as the early warning.
