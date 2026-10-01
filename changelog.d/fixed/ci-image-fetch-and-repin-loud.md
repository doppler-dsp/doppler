- **The CI image fetches fail loudly, and so does a pending repin.**
    Every `curl` in `deploy/docker/Dockerfile.ci` now uses
    `-fsSL --retry 5 --retry-all-errors --retry-delay 2`, and the
    `get-jb.sh` installer is downloaded before it runs instead of piped
    into `bash`. `make lint-curl-fail` refuses a `curl` without `--fail`
    in `deploy/docker/` or `.github/` (#1738). The nightly `ci-image.yml`
    run now ends red while `ci/repin-image` holds an unlanded repin,
    because that ejects every PR in the merge queue (#1737).
