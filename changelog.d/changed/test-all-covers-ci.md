- **`make test-all` runs every test target CI runs.** It now also runs
    `test-snippets`, `test-stubs`, `test-api-docs` and `test-rust`, and a
    lint gate (`make lint-test-all-ci`) fails when a workflow runs a
    `make test-*` target that `test-all` leaves out, unless
    `scripts/.test-all-exempt` says why it cannot run locally (only
    `test-sweep`, whose time budget is calibrated on CI's runner). Step 3.2
    (#2886) was green locally and failed CI twice on checks only the
    doc-fence gates collect.
