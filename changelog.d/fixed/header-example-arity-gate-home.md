- **The header-example arity gate now actually runs.** It was in `LINT_TOOLS`
    and in nothing else — no pre-commit hook, no CI step — and `make lint` runs
    pre-commit, so the only way to execute it was to type
    `make lint-header-example-arity` by hand. Nobody did, for the life of the
    gate, and its 11-finding ratchet was never enforced. Verified by putting a
    deliberate arity error in a header: `make lint` did not run the check at
    all. With the hook it fails, naming both counts. Third instance of a gate
    with no execution home in this repo; see doppler#1104 for the check that
    would catch a fourth.
