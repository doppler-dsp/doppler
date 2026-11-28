# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# The steered-rate bound's premise is asserted at create (doppler#3408):
# dp_ratesync_loop_bind_cascade() aborts a Debug build whose nominal
# terminal rate is at or over the converter's bound. This run forces that
# geometry and passes only by catching SIGABRT; a build without assertions
# (NDEBUG) reports SKIPPED (exit 77), never passed.
add_test(NAME test_ratesync_core_rate_bound_premise_aborts
         COMMAND test_ratesync_core --abort rate-bound-premise)
set_tests_properties(test_ratesync_core_rate_bound_premise_aborts
                     PROPERTIES SKIP_RETURN_CODE 77)
