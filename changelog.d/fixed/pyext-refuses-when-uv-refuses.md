- **`make pyext` no longer builds for the system Python when uv refuses.**
    A uv that refused (most often: not the pinned version) used to fall
    back to `python3` silently; `pyext`, `coverage`, `warnings-check` and
    the downstream Python build now fail with uv's reason, as does an
    empty `PYTHON_EXECUTABLE=`. Only the interpreter probe honors `UV=`
    here; `pyext`'s `uv sync` is #2370's
    ([#2364](https://github.com/doppler-dsp/doppler/issues/2364)).
