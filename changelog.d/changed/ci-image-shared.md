- **CI runs in the org standard's shared toolchain image.** doppler's own
    `Dockerfile.ci`, `ci-image.yml` and source-hash script are replaced by
    canonical's (`HAS_CI_IMAGE`), configured from the Makefile; doppler's
    extras (clang's profile runtime, a checksummed nats-server) move to
    `docker/ci-extra.sh`. A pending weekly repin now keeps `ci-image.yml`
    red on `main` instead of failing every PR, so the `ci-image-repin` job
    is gone; `ci-image-refs-check` keeps the container-ref half of the old
    `ci-image-check`.
