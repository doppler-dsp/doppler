- **A gate on the rule three suites followed and one did not**:
    `make instrumented-sweep-check` fails when a makefile block that builds
    with `-fsanitize=` or `-DDOPPLER_COVERAGE=ON` runs `ctest` without
    excluding the `sweep` label. It reads the makefiles rather than a roster,
    so a fourth instrumented suite is covered without being registered, and it
    resolves through variables — the marker is a level away in `TSAN_FLAGS`,
    and the exclusion a level away in `SAN_EXCLUDE_SWEEP`, which is what lets
    each suite keep its own escape hatch. Wired into `make lint`, and driven
    over seeded makefiles by `test_instrumented_sweep_gate.py` so it is proven
    to go red rather than observed to be green: it is red on the tree as it
    stood before this change, naming the one line.
    [#1292](https://github.com/doppler-dsp/doppler/issues/1292).
