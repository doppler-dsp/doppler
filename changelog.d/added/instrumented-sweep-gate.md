- **`make instrumented-sweep-check`** fails when a makefile block that builds
    with `-fsanitize=` or `-DDOPPLER_COVERAGE=ON` runs `ctest` without
    excluding the `sweep` label. It reads the makefiles rather than a roster,
    so a fourth instrumented suite is covered without being registered.
    [#1292](https://github.com/doppler-dsp/doppler/issues/1292).
