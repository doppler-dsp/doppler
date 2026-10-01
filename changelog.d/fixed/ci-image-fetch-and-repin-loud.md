- **The CI image fetches fail loudly, and so does a pending repin.**
    Every `curl` in `deploy/docker/Dockerfile.ci` now uses
    `-fsSL --retry 5 --retry-all-errors --retry-delay 2`, and the
    `get-jb.sh` installer is downloaded before it runs instead of piped
    into `bash`. `make lint-curl-fail` refuses a `curl` without `--fail`
    in `deploy/docker/` or `.github/` (#1738). The nightly `ci-image.yml`
    run now ends red while `ci/repin-image` holds an unlanded repin,
    because that ejects every PR in the merge queue (#1737).
- **A repin is one file on its base, and the repin gate no longer grafts
    history.** `ci-image-repin-check` fetched `ci/repin-image` with
    `--depth=1` even in a full clone, which made the repin commit look
    parentless locally. That misdiagnosis was made twice. It now fetches
    shallow only in a shallow clone. ci-image.yml's push step runs
    `make ci-image-repin-commit-check`, which refuses a repin that is not
    exactly `.github/ci-images.env` on the run's commit (#1510).
