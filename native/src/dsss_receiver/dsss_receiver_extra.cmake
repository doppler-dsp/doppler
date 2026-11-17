# awgn_core: test_dsss_receiver_core takes its captures from
# native/tests/dp_dsss_test.h, whose noise is the shipped awgn sized by
# dp_awgn_amplitude_for_snr() (doppler#689). The test's alone; the object
# itself does not use it.
target_link_libraries(test_dsss_receiver_core PRIVATE awgn_core)
# And the bench's stream, for the same reason.
target_link_libraries(bench_dsss_receiver_core PRIVATE awgn_core)
