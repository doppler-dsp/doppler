- **A release commit no longer re-runs the whole CI matrix.** A diff that
    is a version bump alone (`make ci-changes`, from `standard.mk`) skips
    the 11 heavy jobs its parent already passed; lint, manifest drift and
    the CI-image pin still run. `CI passed` accepts those skips only on that
    classification (`scripts/ci_passed.py`).
