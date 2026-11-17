# awgn_core: test_dll_core's asynchronous-data stream takes the shipped
# noise at the per-sample SNR its detector is sized for (doppler#689). The
# test's alone; the Dll does not use it.
target_link_libraries(test_dll_core PRIVATE awgn_core)
