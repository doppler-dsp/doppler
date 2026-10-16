- **`UV=` now reaches every uv call in the Makefile.** Passing the pinned
    uv as `UV=` is the way out on a machine whose uv is not the pinned
    version, but 94 Makefile lines ran a bare `uv`, so it did not reach them:
    `make pyext` with `UV=` set still failed at its final `uv sync`. They
    all go through `$(UV)` now, and `make lint-uv-pin` refuses a new bare
    uv call in any makefile. `scripts/.bare-uv-allow`, the list of lines
    that may stay bare, is empty and only shrinks
    ([#2369](https://github.com/doppler-dsp/doppler/issues/2369)).
