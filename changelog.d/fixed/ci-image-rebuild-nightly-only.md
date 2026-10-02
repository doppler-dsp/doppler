- **The CI image no longer rebuilds on `main`.**
    `ci-image.yml`'s push trigger was `branches: ['**']`, so merging a
    Dockerfile change rebuilt an image the PR had already built and pinned.
    The rebuild re-resolved apt against a moved mirror and owed a fresh
    repin, which blocked every PR. It is now `branches-ignore: [main]`.
    Main rebuilds from the weekly run and `workflow_dispatch` only, and a
    feature branch still
    builds the image it pins (#1748).
