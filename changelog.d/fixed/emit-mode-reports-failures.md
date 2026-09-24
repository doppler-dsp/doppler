- **A validation harness's `--emit` mode no longer hides a failed check.**
    `acq_template_pd --emit` returned 0 unconditionally, to keep a banner out
    of its CSV, so a failed `DP_REQUIRE` dropped a section silently and the
    report failed on a parse error two layers away. `DP_TEST_EMIT_END` in
    `dp_test.h` returns the failing status with nothing on stdout.
