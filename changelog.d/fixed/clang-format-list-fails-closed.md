- **`make lint-clang-format` no longer passes while checking fewer files.**
    Its list of hand-written `_ext.c` files came from a bare `uv run`
    whose failure was lost in a pipe, so on a machine whose uv is not the
    pinned version those files silently dropped out and the target still
    exited 0. The list now goes through `$(UV)`, and a failure to produce
    it fails the target
    ([#2361](https://github.com/doppler-dsp/doppler/issues/2361)).
