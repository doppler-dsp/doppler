- **`make lint` checks that no fork can reach a self-hosted runner.**
    Re-vendored `standard.mk` from canonical brings `self-hosted-check`
    and the `ci-runner` matrix filter (`scripts/ci-runner.py`); no
    doppler workflow uses a self-hosted runner yet, so the gate passes
    as is.
