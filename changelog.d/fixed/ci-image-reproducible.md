- **The CI image re-pins weekly, and a rebuild in between reproduces it.**
    The two inputs that moved under an unchanged `Dockerfile.ci`, the base
    image tag and the apt mirror, are build arguments with no default.
    Their values are pinned in `.github/ci-images.env`: `CI_BASE_2204` and
    `CI_BASE_2404` by digest, and `CI_APT_SNAPSHOT`, which every apt source
    is rewritten to on `snapshot.ubuntu.com`. Only the Monday `ci-image.yml`
    run (or a dispatch with `refresh`) picks new values. Every other build
    reads the pin. On 2026-10-01 the old nightly-plus-main rebuilds owed
    three re-pins in one day (#1748).
