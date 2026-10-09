- **CI's Docker, glibc and package legs no longer fail on Docker Hub's rate
    limit.** They pulled stock images by bare Docker Hub name, anonymously,
    and with several PRs in flight the 429 failed unrelated PRs. Every stock
    image now comes through the Makefile's `STOCK_REGISTRY` (ECR Public's
    mirror of the official images: same digests, no credentials), the
    Dockerfiles drop the `# syntax=` frontend pull, and
    `make lint-stock-images` refuses a bare one (#1950).
