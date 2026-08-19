- **`docs/dev/` is organised around its spine, and CI has a page.** The
    thirteen pages [Adding an
    Algorithm](https://doppler-dsp.github.io/doppler/dev/contributing/adding-algorithms/)
    links to now live under `docs/dev/contributing/`, leaving `docs/dev/`
    holding the index and the maintainer-internals pages. The spine was
    already the entry point and `index.md` already listed its members
    separately from maintainer plumbing; the directory now says the same
    thing the index did.

    New: [Continuous Integration](https://doppler-dsp.github.io/doppler/dev/ci/)
    — what CI is made of and how to run it yourself. The pinned toolchain
    image and why it has no package list of its own; the three things
    installed outside `bootstrap.toml` and why a cross-distro list cannot
    express them; digest pinning and what "refresh nightly" does and does not
    mean; the compiler cache and its measured hit rate; and the four gates
    that watch CI itself.

    It leads with `make ci-shell` / `ci-run` / `ci-gates`, because the point
    of baking the toolchain is that "works on my machine" and "works in CI"
    become the same sentence — including the warning that container and host
    build trees must not be mixed, which is a link error that reads as a code
    bug.

    The move itself was gated end to end: `check_nav_index` for the index
    bullets, the strict site build and `check_site_links` for 183 pages of
    internal links, `check_doc_targets` for every `make` target the new page
    names, and `gen_validation_log`'s output path — a generated page, so its
    generator moved with it rather than being left pointing at a path that no
    longer exists.
