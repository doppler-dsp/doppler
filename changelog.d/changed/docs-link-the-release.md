- **The docs link the release you installed, not `main`.** Repository links
    and `git clone` lines in the pages are pinned to the release tag and
    re-stamped on every release; `make lint` refuses a `blob/main` link or an
    unpinned clone outside the contributing pages, and checks each pinned link
    against the tag. See
    [#2857](https://github.com/doppler-dsp/doppler/issues/2857).
