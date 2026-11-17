# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# awgn_core and gold_core: the bench feeds the pool the shipped noise
# generator and the CCSDS default Gold code, and nothing else here uses
# them. Linked onto the bench alone, as the receiver's own extra cmake does,
# so their headers stay out of the pool's public one.
target_link_libraries(bench_async_dsss_pool_core PRIVATE awgn_core gold_core
                                                         wfm_synth_core)

# awgn_core and wfm_synth_core: the ghost tests' 415.1-B-1 emitter is the
# use-case scene, native/tests/dp_air_test.h (the shipped synth with its
# code-only window, the front end's converter to 11 MSa/s, a DopplerChannel,
# the shipped noise; _test_ghost_is_released). The test's alone, as above.
target_link_libraries(test_async_dsss_pool_core PRIVATE awgn_core
                                                        wfm_synth_core)
