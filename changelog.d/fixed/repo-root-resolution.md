- **Tests found the repo root by counting directories, so 89 of them failed
    under the coverage harness.** `Path(__file__).parents[N]` is correct
    exactly once — from the source checkout. `make coverage` copies the
    package to `build-cov/pkg/doppler`, two levels deeper, so every
    fixed-depth root landed inside `build-cov/` and every test reading
    `docs/`, `scripts/` or `native/` failed there.

    It went unnoticed because `COVERAGE_CMD`'s pytest carries a leading `-`,
    so make ignores its exit code: **27 failed and 62 errors against 2631
    passed**, permanently tolerated in the one job that produces the coverage
    number. `wfm/tests/test_schema.py` alone contributed 47, because
    `parents[4]` missed `docs/schema/wfmgen.schema.json` and took out a
    module-scope fixture.

    `doppler.tests._repo.repo_root()` walks up for `pyproject.toml` **and**
    `native/` together — either alone could match an unrelated parent project
    — which is depth-independent and therefore right from both trees. 16
    sites converted.

    The walk is deliberately better than skipping those tests: `build-cov/`
    lives *inside* the repo, so it finds the real root and they now **run**
    under coverage and contribute to the number, rather than being excluded
    as "needs a checkout".

    `test_benchmark_fixtures.py` keeps its `parents[1]`, which means
    `src/doppler` rather than the repo root and was never wrong.

    Verified against both trees: the source-tree suite is unchanged at 2719
    passed / 7 skipped, and the fixed-depth idiom is gone from every site
    that meant "repo root".
