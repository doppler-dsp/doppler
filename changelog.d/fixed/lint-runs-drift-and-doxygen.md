- **`make lint` now runs `drift-check` and `doxygen-check`, so a header edit
    is gated locally.** A header feeds three generators and only one of the
    three was reachable from `lint`, so a doxygen `@param` fix could leave a
    `.pyi` stale, pass every local target, and fail CI on the one gate the
    sweep omitted ([#1171](https://github.com/doppler-dsp/doppler/issues/1171)).
    Lint goes 32 s to ~77 s; `doxygen-check` runs the CI image under Docker
    unless local doxygen is exactly 1.9.8.
