- **`make test-asan` takes `CTEST_SELECT`**, like `make test`:
    `make test-asan CTEST_SELECT='-R test_correlator'` runs 4 tests under
    ASan instead of 168. A selection that matches nothing fails rather than
    passing over nothing.
