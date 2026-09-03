- **The CI toolchain image is now published for `linux/amd64` AND
    `linux/arm64`.** It was amd64-only, so `docker run $(CI_IMAGE)` --
    `gen-c-api-check`, `ci-shell`, `ci-run`, and `container:` in ci.yml,
    which all pin the same digest -- failed with `exec format error` on an
    arm64 dev box or runner. `ci-image.yml` now builds each base with
    buildx + QEMU, the same pattern already used to publish the
    runtime/SDK/downstream-jm images.
