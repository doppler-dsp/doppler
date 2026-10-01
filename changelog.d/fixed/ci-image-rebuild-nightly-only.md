- **The CI image no longer rebuilds on `main` or in the merge queue.**
    `ci-image.yml`'s push trigger was `branches: ['**']`, so merging a
    Dockerfile change rebuilt an image the PR had already built and pinned.
    The rebuild re-resolved apt against a moved mirror and owed a fresh
    repin, which ejected the whole queue. It is now
    `branches-ignore: [main, 'gh-readonly-queue/**']`. Main rebuilds from
    the nightly and `workflow_dispatch` only, and a feature branch still
    builds the image it pins (#1748).
