- **A test that accumulates failures and never reports them is now a lint
    failure.** `DP_CHECK` counts into `dp_test_fails_` and carries on; only
    `DP_TEST_END` turns that counter into a non-zero exit. A file ending with
    its own `printf ("… OK …")` and `return 0` therefore runs every check,
    records every failure, and exits 0 — so each of its `DP_CHECK`s is
    decoration and `ctest` reports the test as passing while it asserts
    nothing that can fail.

    Two files ended that way, and they show the two different costs.
    `test_frame_meter_core.c` shipped **3 `DP_CHECK`s that could not fail**.
    `test_wfm_frame.c` asserted entirely through `DP_REQUIRE`, which *does*
    return 1, so its epilogue was latent rather than broken — and went off the
    moment 12 `DP_CHECK`s were added to it. That is the worse half: the file
    looked healthy, and writing an ordinary assertion into it produced an
    assertion that could not fail.

    Found by sabotage rather than by review. Making `wfm_frame_t`'s
    `preamble_reps = 0` emit one period instead of no preamble changed the
    layout, and every test still passed. It is precisely the shape
    `DP_TEST_END`'s own "ASSERTED NOTHING" guard exists to catch and cannot,
    because a file that never calls it never runs that guard either.

    Both epilogues now call `DP_TEST_END`, and `check_tests_ssot.py` refuses
    any test using the accumulating flavour without it. The rule is absolute
    rather than a ratchet — after the fix the count is zero — and
    registration-free, so a new test scaffolded from an old template is
    covered the moment it lands. Proven by putting the defect back and
    watching `make tests-ssot` name the file.
