- **The patch-coverage gate runs a pinned diff-cover.** The
    `coverage-gate` target ran `uvx diff-cover`, which resolves the tool
    outside `uv.lock`, so its verdict could change on the day diff-cover
    released, with no commit here. diff-cover 10.6.0 is now an exact pin
    in the dev group and runs through the lock, and `make lint-uv-pin`
    refuses `uvx`, `uv tool run` or `uv run --with` in any makefile
    ([#2371](https://github.com/doppler-dsp/doppler/issues/2371)).
