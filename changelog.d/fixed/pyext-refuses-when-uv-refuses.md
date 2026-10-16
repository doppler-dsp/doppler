- **`make pyext` no longer builds for the system Python when uv refuses.**
    When uv was installed but refused to run (most often: not the pinned
    version), the interpreter choice quietly fell back to `python3` from
    `PATH`, so the extension was built for the wrong interpreter. `pyext`,
    `coverage` and `warnings-check` now fail with uv's reason and the two
    ways out (`UV=` the pinned uv, or `PYTHON_EXECUTABLE=`). The `python3`
    fallback still applies on a machine with no uv at all, and `UV=` is now
    honoured
    ([#2364](https://github.com/doppler-dsp/doppler/issues/2364)).
