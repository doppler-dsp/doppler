- **The CI image's package installer is pinned too.** `Dockerfile.ci` ran
    `get-jb.sh` and `jbx install-deps`, which always took the newest
    just-bashit, including the newest install logic, resolved at run time.
    It now runs `install-deps.sh` from a pinned just-bashit release tarball
    (`CI_JB_VERSION`, `CI_JB_SHA256` in `.github/ci-images.env`), verified
    by checksum and re-picked only by the weekly re-pin. The dry-run
    install plan is byte-identical to before (#1751).
