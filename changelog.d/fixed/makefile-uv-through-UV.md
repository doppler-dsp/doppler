- **`UV=` now reaches every uv call in the Makefile.** Passing the pinned
    uv as `UV=` is the way out on a machine whose uv is not the pinned
    version, but 92 Makefile lines ran a bare `uv`, so it did not reach them:
    `make pyext` with `UV=` set still failed at its final `uv sync`. They
    now go through `$(UV)`, and `make lint-uv-pin` refuses a new bare
    `uv run` or `uv sync` in any makefile; the two lines still bare are
    listed, with their reasons, in `scripts/.bare-uv-allow`
    ([#2369](https://github.com/doppler-dsp/doppler/issues/2369)).
