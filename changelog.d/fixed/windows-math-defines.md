- **A Windows consumer of the installed C package compiles the headers.**
    `_USE_MATH_DEFINES` now travels with the package (the exported CMake
    target and `doppler.pc`) as `_GNU_SOURCE` does on Linux, so the headers'
    inline `M_PI` / `M_SQRT2` resolve under the UCRT; it had been set for
    doppler's own build only.
