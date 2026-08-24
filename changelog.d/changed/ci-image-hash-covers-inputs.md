- **The CI image fingerprint hashes the image's inputs, not two whole files.**
    `ci-image-source-hash` was `cat bootstrap.toml Dockerfile.ci | sha256sum`,
    which hashes the *files* rather than the *image*: every byte counted,
    including `bootstrap.toml`'s `[project] version`, a string no layer reads.
    That is what had kept doppler's version out of `bootstrap.toml` — syncing
    it would have invalidated the image on every release and demanded a rebuild
    plus a repin for nothing. Measured before changing anything: editing that
    one line failed `make ci-image-check`; so did reformatting a comment.

    `scripts/ci_image_source_hash.py` parses `bootstrap.toml`, drops
    `[project]`, and hashes the remaining tables by value alongside the
    Dockerfile. Excluding one reasoned table beats listing the ones to include:
    an include-list silently stops covering a table added later, and forgetting
    should cost an extra rebuild, not a missed one. Parsing also drops comments
    and whitespace from the hash — a note above a package list is not a
    different image.

    Six mutations, all measured: a bumped `[project] version`, an appended
    comment and a changed `[project] name` leave the hash alone; an added
    `[dev.apt]` package, a changed `[tools.install-deps] groups` and a touched
    `Dockerfile.ci` all move it.

    `ci-image.yml` now calls `make -s ci-image-source-hash` instead of spelling
    the derivation a second time, so the workflow that writes the pin and the
    gate that checks it cannot compute different numbers. The recipe uses plain
    `python3` rather than `uv run python`: that workflow builds the image the
    rest of CI runs inside, so it is the one place that cannot assume a synced
    uv, and the script is stdlib-only.
