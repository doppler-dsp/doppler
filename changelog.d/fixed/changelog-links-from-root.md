- **A changelog fragment's links are checked where they will be read.**
    `check_doc_paths` and `check_doc_sections` now resolve a `changelog.d/`
    fragment's links from the repository root, where `CHANGELOG.md` holds them.
    A `../../docs/...` link fails on its own PR, not on the release commit,
    which is where v0.74.0's did. The 16 links in `CHANGELOG.md` that already
    point nowhere sit on a list that may only shrink
    ([#2571](https://github.com/doppler-dsp/doppler/issues/2571)).
