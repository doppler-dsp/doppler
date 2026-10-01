- **A pull request now runs the doc-fence gates and `make validate-check`.**
    The `python` job's single-leg steps chose their leg with a literal
    `'3.12'`, and a pull request runs only 3.9, so they ran in the merge
    queue alone. They now name the primary leg by role, the floor, from
    `scripts/python_versions.py --primary`. The cheap gates run on every
    run; the coverage run stays in the queue. `make ci-aggregator-check`
    refuses a version literal in a leg selector. See `docs/dev/ci.md`
    (#1714).
