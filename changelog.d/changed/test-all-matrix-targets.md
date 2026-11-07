- **`make lint-test-all-ci` reads matrix-chosen targets** (#2912): a
    `make ${{ matrix.<path> }}` step is resolved against its job's
    `strategy.matrix`, so the sanitizer job's `test-asan`, `test-ubsan` and
    `test-tsan` are seen. Each is exempt from `test-all` with its reason:
    it builds its own instrumented tree, and CI is its gate.
