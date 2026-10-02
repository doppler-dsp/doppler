- **The `python` job's single-leg steps name their leg by role.** The
    doc-fence gates, `make validate-check` and the coverage run chose their
    leg with a literal `'3.12'`, which would match no leg once 3.12 left the
    classifiers. They now run on the primary leg, the floor, from
    `scripts/python_versions.py --primary`. `make ci-aggregator-check`
    refuses a version literal in a leg selector. See `docs/dev/ci.md`
    (#1714).
